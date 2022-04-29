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

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/optional_debug_tools.h"

#include "tflite/public/edgetpu.h"

#define LOG(x) std::cerr

namespace wrapper_tfl {

	double get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

	struct Config {
		bool verbose;
		std::string model_name;
	};

	class Tfl_WrapperEdgeTPU {
	private:
		// Taking a reference to the (const) model data avoids lifetime-related issues
		// and complexity with the TFL_Model's existence.
		std::shared_ptr<edgetpu::EdgeTpuContext> m_edgetpu_ctx;
		std::unique_ptr<tflite::FlatBufferModel> m_model;
		std::unique_ptr<tflite::Interpreter>     m_interpreter;
		bool                                     m_verbose;
		bool                                     m_inputFloating;
		float                                    m_inferenceTime;

	public:
		Tfl_WrapperEdgeTPU() {}

		void Initialize(Config* conf)
		{
			m_inputFloating = false;
			m_inferenceTime = 0;
			m_verbose = conf->verbose;

			if (!conf->model_name.c_str()) {
				LOG(ERROR) << "no model file name\n";
				exit(-1);
			}

			m_edgetpu_ctx = edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();

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

			resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());

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

			interpreter->SetExternalContext(kTfLiteEdgeTpuContext,
							m_edgetpu_ctx.get());
			interpreter->SetNumThreads(1);

			m_interpreter = std::move(interpreter);
			m_model = std::move(model);
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

			auto in = m_interpreter->typed_tensor<uint8_t>(input);
			for (int i = 0; i < sizeInBytes; i++)
				in[i] = img[i];

			struct timeval start_time, stop_time;
			gettimeofday(&start_time, nullptr);
			if (m_interpreter->Invoke() != kTfLiteOk) {
				LOG(FATAL) << "Failed to invoke tflite!\n";
			}
			gettimeofday(&stop_time, nullptr);
			m_inferenceTime = (get_us(stop_time) - get_us(start_time));

			return m_inferenceTime;
		}
	};

}  // namespace wrapper_tfl

#endif  // WRAPPER_TFL_H_
