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

#ifndef WRAPPER_TFL_H_
#define WRAPPER_TFL_H_

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

struct ObjDetect_Location {
	float y0, x0, y1, x1;
};

struct ObjDetect_Results {
	float score[10];
	int index[10];
	struct ObjDetect_Location location[10];
	float inference_time;
};

int GetInputWidth(Interpreter* interpreter);
int GetInputHeight(Interpreter* interpreter);
int GetInputChannels(Interpreter* interpreter);
void DisplayModelInformation(Interpreter* interpreter);
Interpreter* InitInterpreter(Config* conf);
void RunInference(Config* conf, Interpreter* interpreter, uint8_t* img);
void GetObjDetectResults(Config* conf, Interpreter* interpreter, ObjDetect_Results* results);
TfLiteStatus ReadLabelsFile(const std::string& file_name,
			    std::vector<std::string>* result,
			    size_t* found_label_count);
}  // namespace wrapper_tfl
}  // namespace tflite

#endif  // WRAPPER_TFL_H_
