/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <fcntl.h>      // NOLINT(build/include_order)
#include <getopt.h>     // NOLINT(build/include_order)
#include <sys/time.h>   // NOLINT(build/include_order)
#include <sys/types.h>  // NOLINT(build/include_order)
#include <sys/uio.h>    // NOLINT(build/include_order)
#include <unistd.h>     // NOLINT(build/include_order)

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "wrapper_tfl.h"

#define LOG(x) std::cerr

namespace tflite {
namespace wrapper_tfl {

double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

void PrintProfilingInfo(const profiling::ProfileEvent* e, uint32_t op_index,
			TfLiteRegistration registration) {
	// output something like
	// time (ms) , Node xxx, OpCode xxx, symblic name
	//      5.352, Node   5, OpCode   4, DEPTHWISE_CONV_2D

	LOG(INFO) << std::fixed << std::setw(10) << std::setprecision(3)
		<< (e->end_timestamp_us - e->begin_timestamp_us) / 1000.0
		<< ", Node " << std::setw(3) << std::setprecision(3) << op_index
		<< ", OpCode " << std::setw(3) << std::setprecision(3)
		<< registration.builtin_code << ", "
		<< EnumNameBuiltinOperator(static_cast<BuiltinOperator>(registration.builtin_code))
		<< "\n";
}

int TfLiteGetInputWidth(TfL_Interpreter* interpreter)
{
	int input = interpreter->impl->inputs()[0];
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	return input_dims->data[2];
}

int TfLiteGetInputHeight(TfL_Interpreter* interpreter)
{
	int input = interpreter->impl->inputs()[0];
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	return input_dims->data[1];
}

int TfLiteGetInputChannels(TfL_Interpreter* interpreter)
{
	int input = interpreter->impl->inputs()[0];
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	return input_dims->data[3];
}


void TfLiteDisplaySettings(TfL_Config* conf)
{
	LOG(INFO) << "accel             " << conf->accel << "\n";
	LOG(INFO) << "verbose           " << conf->verbose << "\n";
	LOG(INFO) << "input_floating    " << conf->input_floating << "\n";
	LOG(INFO) << "profiling         " << conf->profiling << "\n";
	LOG(INFO) << "allow_fp16        " << conf->allow_fp16 << "\n";
	LOG(INFO) << "loop_count        " << conf->loop_count << "\n";
	LOG(INFO) << "input_mean        " << conf->input_mean << "\n";
	LOG(INFO) << "input_std         " << conf->input_std << "\n";
	LOG(INFO) << "number_of_threads " << conf->number_of_threads << "\n";
	LOG(INFO) << "number_of_results " << conf->number_of_results << "\n";
	LOG(INFO) << "model_name        " << conf->model_name << "\n";
}

TfL_Interpreter* TfLiteInitInterpreter(TfL_Config* conf)
{
	if (!conf->model_name.c_str()) {
		LOG(ERROR) << "no model file name\n";
		exit(-1);
	}

	std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> interpreter;
	model = tflite::FlatBufferModel::BuildFromFile(conf->model_name.c_str());
	if (!model) {
		LOG(FATAL) << "\nFailed to mmap model " << conf->model_name << "\n";
		exit(-1);
	}
	LOG(INFO) << "Loaded model " << conf->model_name << "\n";
	model->error_reporter();
	LOG(INFO) << "resolved reporter\n";

	tflite::ops::builtin::BuiltinOpResolver resolver;

	tflite::InterpreterBuilder(*model, resolver)(&interpreter);
	if (!interpreter) {
		LOG(FATAL) << "Failed to construct interpreter\n";
		exit(-1);
	}

	interpreter->UseNNAPI(conf->accel);
	interpreter->SetAllowFp16PrecisionForFp32(conf->allow_fp16);

	if (conf->number_of_threads != -1) {
		interpreter->SetNumThreads(conf->number_of_threads);
	}

	return new TfL_Interpreter{std::move(model), std::move(interpreter)};
}

void TfLiteDisplayModelInformation(TfL_Interpreter* interpreter)
{
	LOG(INFO) << "tensors size: " << interpreter->impl->tensors_size() << "\n";
	LOG(INFO) << "nodes size: " << interpreter->impl->nodes_size() << "\n";
	LOG(INFO) << "inputs: " << interpreter->impl->inputs().size() << "\n";
	LOG(INFO) << "input(0) name: " << interpreter->impl->GetInputName(0) << "\n";

	int t_size = interpreter->impl->tensors_size();
	for (int i = 0; i < t_size; i++) {
		if (interpreter->impl->tensor(i)->name)
			LOG(INFO) << i << ": " << interpreter->impl->tensor(i)->name << ", "
				<< interpreter->impl->tensor(i)->bytes << ", "
				<< interpreter->impl->tensor(i)->type << ", "
				<< interpreter->impl->tensor(i)->params.scale << ", "
				<< interpreter->impl->tensor(i)->params.zero_point << "\n";
	}
}

void TfLiteRunInference(TfL_Config* conf, TfL_Interpreter* interpreter, uint8_t* img)
{
	int input = interpreter->impl->inputs()[0];
	if (conf->verbose) LOG(INFO) << "input: " << input << "\n";

	if (conf->verbose) {
		const std::vector<int> inputs = interpreter->impl->inputs();
		const std::vector<int> outputs = interpreter->impl->outputs();
		LOG(INFO) << "number of inputs: " << inputs.size() << "\n";
		LOG(INFO) << "number of outputs: " << outputs.size() << "\n";
	}

	if (interpreter->impl->AllocateTensors() != kTfLiteOk) {
		LOG(FATAL) << "Failed to allocate tensors!";
	}

	if (conf->verbose) PrintInterpreterState(interpreter->impl.get());

	// get input dimension from the input tensor metadata
	// assuming one input only
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	int input_height = input_dims->data[1];
	int input_width = input_dims->data[2];
	int input_channels = input_dims->data[3];

	if (interpreter->impl->tensor(input)->type == kTfLiteFloat32)
		conf->input_floating = true;

	auto output_number_of_pixels = input_height * input_width * input_channels;
	if (conf->input_floating) {
		auto in = interpreter->impl->typed_tensor<float>(input);
		for (int i = 0; i < output_number_of_pixels; i++)
			in[i] = img[i];
	} else {
		auto in = interpreter->impl->typed_tensor<uint8_t>(input);
		for (int i = 0; i < output_number_of_pixels; i++)
			in[i] = img[i];
	}

	profiling::Profiler* profiler = new profiling::Profiler();
	interpreter->impl->SetProfiler(profiler);

	if (conf->profiling) profiler->StartProfiling();

	struct timeval start_time, stop_time;
	gettimeofday(&start_time, nullptr);
	for (int i = 0; i < conf->loop_count; i++) {
		if (interpreter->impl->Invoke() != kTfLiteOk) {
			LOG(FATAL) << "Failed to invoke tflite!\n";
		}
	}

	gettimeofday(&stop_time, nullptr);
	interpreter->inference_time = (get_ms(stop_time) - get_ms(start_time)) / (conf->loop_count);
	//LOG(INFO) << "invoked \n";
	//LOG(INFO) << "average time: " << interpreter->inference_time << " ms\n";

	if (conf->profiling) {
		profiler->StopProfiling();
		auto profile_events = profiler->GetProfileEvents();
		for (unsigned int i = 0; i < profile_events.size(); i++) {
			auto op_index = profile_events[i]->event_metadata;
			const auto node_and_registration =
				interpreter->impl->node_and_registration(op_index);
			const TfLiteRegistration registration = node_and_registration->second;
			PrintProfilingInfo(profile_events[i], op_index, registration);
		}
	}
}

void TfLiteGetLabelResults(TfL_Config* conf, TfL_Interpreter* interpreter, TfL_Label_Results* results)
{

	const float threshold = 0.001f;

	std::vector<std::pair<float, int>> top_results;

	int output = interpreter->impl->outputs()[0];
	TfLiteIntArray* output_dims = interpreter->impl->tensor(output)->dims;
	// assume output dims to be something like (1, 1, ... ,size)
	auto output_size = output_dims->data[output_dims->size - 1];
	if (conf->input_floating) {
		get_top_n<float>(interpreter->impl->typed_output_tensor<float>(0),
				 output_size, conf->number_of_results, threshold,
				 &top_results, true);
	} else {
		get_top_n<uint8_t>(interpreter->impl->typed_output_tensor<uint8_t>(0),
				   output_size, conf->number_of_results, threshold,
				   &top_results, false);
	}

	int i = 0;
	for (const auto& result : top_results) {
		results->accuracy[i] = result.first;
		results->index[i] = result.second;
		i++;
	}
	results->inference_time = interpreter->inference_time;
}

void TfLiteGetObjDetectResults(TfL_Config* conf, TfL_Interpreter* interpreter, TfL_ObjDetect_Results* results)
{
	// location
	float *locations = interpreter->impl->typed_output_tensor<float>(0);
	// classes
	float *classes = interpreter->impl->typed_output_tensor<float>(1);
	// score
	float *scores = interpreter->impl->typed_output_tensor<float>(2);

	// get the output size
	int output = interpreter->impl->outputs()[2];
	TfLiteIntArray* output_dims = interpreter->impl->tensor(output)->dims;
	// assume output dims to be something like (1, 1, ... ,size)
	auto output_size = output_dims->data[output_dims->size - 1];

	// the outputs are already sort by descending order
	for (int i = 0; i < output_size; i++) {
		results->score[i]       = scores[i];
		results->index[i]       = (int)classes[i];
		results->location[i].y0 = locations[(i * 4) + 0];
		results->location[i].x0 = locations[(i * 4) + 1];
		results->location[i].y1 = locations[(i * 4) + 2];
		results->location[i].x1 = locations[(i * 4) + 3];
	}
	results->inference_time = interpreter->inference_time;
}


// Takes a file name, and loads a list of labels from it, one per line, and
// returns a vector of the strings. It pads with empty strings so the length
// of the result is a multiple of 16, because our model expects that.
TfLiteStatus ReadLabelsFile(const string& file_name,
			    std::vector<string>* result,
			    size_t* found_label_count)
{
	std::ifstream file(file_name);
	if (!file) {
		LOG(FATAL) << "Labels file " << file_name << " not found\n";
		return kTfLiteError;
	}
	result->clear();
	string line;
	while (std::getline(file, line)) {
		result->push_back(line);
	}
	*found_label_count = result->size();
	const int padding = 16;
	while (result->size() % padding) {
		result->emplace_back();
	}
	return kTfLiteOk;
}

}  // namespace wrapper_tfl
}  // namespace tflite
