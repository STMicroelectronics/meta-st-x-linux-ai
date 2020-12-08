/*
 * wrapper_tfl_edgetpu.hpp
 *
 * Author: Vincent Abriou <vincent.abriou@st.com> for STMicroelectronics.
 *
 * Copyright (c) 2020 STMicroelectronics. All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *
 *     http://www.opensource.org/licenses/BSD-3-Clause
 *
 *
 *
 * Inspired by:
 * https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/examples/label_image
 * Copyright 2017 The TensorFlow Authors. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef WRAPPER_TFL_H_
#define WRAPPER_TFL_H_

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
#include <unordered_set>

#include <algorithm>
#include <functional>
#include <queue>
#include <memory>
#include <string>
#include <vector>

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "tensorflow/lite/profiling/profiler.h"

#include "tensorflow/lite/edgetpu.h"


#define LOG(x) std::cerr

namespace tflite {
namespace wrapper_tfl {

struct Config {
	bool verbose = false;
	bool accel = false;
	bool input_floating = false;
	bool profiling = false;
	bool allow_fp16 = false;
	int loop_count = 1;
	float input_mean = 127.5f;
	float input_std = 127.5f;
	int number_of_threads = 4;
	int number_of_results = 5;
	std::string model_name;
	std::string labels_file_name;
};

struct Interpreter {
	// Taking a reference to the (const) model data avoids lifetime-related issues
	// and complexity with the TFL_Model's existence.
	std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> impl;
	float inference_time;
};


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

int GetInputWidth(Interpreter* interpreter)
{
	int input = interpreter->impl->inputs()[0];
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	return input_dims->data[2];
}

int GetInputHeight(Interpreter* interpreter)
{
	int input = interpreter->impl->inputs()[0];
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	return input_dims->data[1];
}

int GetInputChannels(Interpreter* interpreter)
{
	int input = interpreter->impl->inputs()[0];
	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	return input_dims->data[3];
}


void DisplaySettings(Config* conf)
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

Interpreter* InitInterpreter(Config* conf, std::shared_ptr<edgetpu::EdgeTpuContext>* edgetpu_context)
{
	if (!conf->model_name.c_str()) {
		LOG(ERROR) << "no model file name\n";
		exit(-1);
	}
	int j = 0;
	tflite::ops::builtin::BuiltinOpResolver resolver;

	std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> interpreter;
	model = tflite::FlatBufferModel::BuildFromFile(conf->model_name.c_str());
	if (!model) {
		LOG(FATAL) << "\nFailed to mmap model " << conf->model_name << "\n";
		exit(-1);
	}
	model->error_reporter();

	resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());
	tflite::InterpreterBuilder(*model, resolver)(&interpreter);
	if (!interpreter) {
		LOG(FATAL) << "Failed to build interpreter\n";
		exit(-1);
	}
	int input = interpreter->inputs()[0];
	if (interpreter->tensor(input)->type == kTfLiteFloat32) {
		conf->input_floating = true;
		LOG(INFO) << "Floating point Tensorflow Lite Model\n";
	}
	interpreter->SetExternalContext(kTfLiteEdgeTpuContext, edgetpu_context->get());
	interpreter->SetNumThreads(1);
	interpreter->AllocateTensors();
	return new Interpreter{std::move(model), std::move(interpreter)};
}

void DisplayModelInformation(Interpreter* interpreter)
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

void RunInference(Config* conf, Interpreter* interpreter,
						std::shared_ptr<edgetpu::EdgeTpuContext>* edgetpu_context,  uint8_t* img)
{
	//TfLiteDisplayModelInformation(interpreter);
	interpreter->impl->SetExternalContext(kTfLiteEdgeTpuContext, edgetpu_context->get());
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

	TfLiteIntArray* input_dims = interpreter->impl->tensor(input)->dims;
	int input_height = input_dims->data[1];
	int input_width = input_dims->data[2];
	int input_channels = input_dims->data[3];


	auto output_number_of_pixels = input_height * input_width * input_channels;
	//LOG(INFO) << "Number of pixels\n";
	if (conf->input_floating) {
		auto in = interpreter->impl->typed_tensor<float>(input);
		for (int i = 0; i < output_number_of_pixels; i++){
			in[i] = (img[i] - conf->input_mean) / conf-> input_std;
		}
	} else {
		auto in = interpreter->impl->typed_tensor<uint8_t>(input);
		for (int i = 0; i < output_number_of_pixels; i++){
			in[i] = img[i];
		}
	}

	profiling::Profiler* profiler = new profiling::Profiler();
	interpreter->impl->SetProfiler(profiler);


	if (conf->profiling) profiler->StartProfiling();
	//TfLiteDisplayModelInformation(interpreter);

	for (int i = 0; i < conf->loop_count; i++) {
		if (interpreter->impl->Invoke() != kTfLiteOk) {
			LOG(FATAL) << "Failed to invoke tflite!\n";
		}
	}
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

}  // namespace wrapper_tfl
}  // namespace tflite

#endif  // WRAPPER_TFL_H_
