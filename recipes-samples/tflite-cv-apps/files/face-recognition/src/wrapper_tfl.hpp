/*
 * wrapper_tfl.hpp
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

#ifndef WRAPPER_TFL_HPP_
#define WRAPPER_TFL_HPP_

#include <algorithm>
#include <functional>
#include <queue>
#include <memory>
#include <string>
#include <sys/time.h>
#include <vector>

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"

#define LOG(x) std::cerr

namespace wrapper_tfl {

	double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

	struct Config {
		bool verbose;
		float input_mean;
		float input_std;
		int number_of_threads;
		std::string model_name;
	};

	class Tfl_Wrapper {
	private:
		// Taking a reference to the (const) model data avoids lifetime-related issues
		// and complexity with the TFL_Model's existence.
		std::unique_ptr<tflite::FlatBufferModel> m_model;
		std::unique_ptr<tflite::Interpreter>     m_interpreter;
		bool                                     m_verbose;
		bool                                     m_inputFloating;
		bool                                     m_allow_fp16;
		bool                                     m_accel;
		float                                    m_inputMean;
		float                                    m_inputStd;
		float                                    m_inferenceTime;
		int                                      m_numberOfThreads;

	public:
		Tfl_Wrapper() {}

		void Initialize(Config* conf)
		{
			m_inputFloating = false;
			m_allow_fp16 = false;
			m_accel = false;
			m_inferenceTime = 0;
			m_verbose = conf->verbose;
			m_inputMean = conf->input_mean;
			m_inputStd = conf->input_std;
			m_numberOfThreads = conf->number_of_threads;

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

			int input = interpreter->inputs()[0];
			if (interpreter->tensor(input)->type == kTfLiteFloat32) {
				m_inputFloating = true;
				LOG(INFO) << "Floating point Tensorflow Lite Model\n";
			}

			interpreter->UseNNAPI(m_allow_fp16);
			interpreter->SetAllowFp16PrecisionForFp32(m_allow_fp16);

			if (m_numberOfThreads != -1) {
				interpreter->SetNumThreads(m_numberOfThreads);
			}

			m_interpreter = std::move(interpreter);
			m_model = std::move(model);
		}

		void DisplaySettings()
		{
			LOG(INFO) << "accel             " << m_accel << "\n";
			LOG(INFO) << "input_floating    " << m_inputFloating << "\n";
			LOG(INFO) << "allow_fp16        " << m_allow_fp16 << "\n";
			LOG(INFO) << "input_mean        " << m_inputMean << "\n";
			LOG(INFO) << "input_std         " << m_inputStd << "\n";
			LOG(INFO) << "number_of_threads " << m_numberOfThreads << "\n";
		}

		void DisplayModelInformation()
		{
			LOG(INFO) << "tensors size: " << m_interpreter->tensors_size() << "\n";
			LOG(INFO) << "nodes size: " << m_interpreter->nodes_size() << "\n";
			LOG(INFO) << "inputs: " << m_interpreter->inputs().size() << "\n";
			LOG(INFO) << "input(0) name: " << m_interpreter->GetInputName(0) << "\n";

			int t_size = m_interpreter->tensors_size();
			for (int i = 0; i < t_size; i++) {
				if (m_interpreter->tensor(i)->name)
					LOG(INFO) << i << ": " << m_interpreter->tensor(i)->name << ", "
						<< m_interpreter->tensor(i)->bytes << ", "
						<< m_interpreter->tensor(i)->type << ", "
						<< m_interpreter->tensor(i)->params.scale << ", "
						<< m_interpreter->tensor(i)->params.zero_point << "\n";
			}
		}

		bool IsModelQuantized()
		{
			return !m_inputFloating;
		}

		int GetInputWidth()
		{
			int input = m_interpreter->inputs()[0];
			TfLiteIntArray* input_dims = m_interpreter->tensor(input)->dims;
			return input_dims->data[2];
		}

		int GetInputHeight()
		{
			int input = m_interpreter->inputs()[0];
			TfLiteIntArray* input_dims = m_interpreter->tensor(input)->dims;
			return input_dims->data[1];
		}

		int GetInputChannels()
		{
			int input = m_interpreter->inputs()[0];
			TfLiteIntArray* input_dims = m_interpreter->tensor(input)->dims;
			return input_dims->data[3];
		}

		unsigned int GetNumberOfInputs()
		{
			const std::vector<int> inputs = m_interpreter->inputs();
			return inputs.size();
		}

		unsigned int GetNumberOfOutputs()
		{
			const std::vector<int> outputs = m_interpreter->outputs();
			return outputs.size();
		}

		unsigned int GetOutputSize(int index)
		{
			int output = m_interpreter->outputs()[index];
			TfLiteIntArray* output_dims = m_interpreter->tensor(output)->dims;
			// assume output dims to be something like (1, 1, ... ,size)
			return output_dims->data[output_dims->size - 1];
		}

		template <class T>
		T* GetOutputTensor(unsigned int index)
		{
			 return m_interpreter->typed_output_tensor<T>(index);
		}

		float RunInference(uint8_t* img)
		{
			if (m_inputFloating)
				return RunInference<float>(img);
			else
				return RunInference<uint8_t>(img);
		}

		template <class T>
		float RunInference(uint8_t* img)
		{
			int input_height = GetInputHeight();
			int input_width = GetInputWidth();
			int input_channels = GetInputChannels();
			auto sizeInBytes = input_height * input_width * input_channels;

			int input = m_interpreter->inputs()[0];
			if (m_verbose) {
				LOG(INFO) << "input: " << input << "\n";
				LOG(INFO) << "number of inputs: " << GetNumberOfInputs() << "\n";
				LOG(INFO) << "number of outputs: " << GetNumberOfOutputs() << "\n";
			}

			if (m_interpreter->AllocateTensors() != kTfLiteOk) {
				LOG(FATAL) << "Failed to allocate tensors!";
			}

			if (m_verbose)
				tflite::PrintInterpreterState(m_interpreter.get());

			auto in = m_interpreter->typed_tensor<T>(input);
			if (m_inputFloating) {
				for (int i = 0; i < sizeInBytes; i++)
					in[i] = (img[i] - m_inputMean) / m_inputStd;
			} else {
				for (int i = 0; i < sizeInBytes; i++)
					in[i] = img[i];
			}

			struct timeval start_time, stop_time;
			gettimeofday(&start_time, nullptr);
			if (m_interpreter->Invoke() != kTfLiteOk) {
				LOG(FATAL) << "Failed to invoke tflite!\n";
			}

			gettimeofday(&stop_time, nullptr);
			m_inferenceTime = (get_ms(stop_time) - get_ms(start_time));

			return m_inferenceTime;
		}
	};
}  // namespace wrapper_tfl

#endif  // WRAPPER_TFL_HPP_
