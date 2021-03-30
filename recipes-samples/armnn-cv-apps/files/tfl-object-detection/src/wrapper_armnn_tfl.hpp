/*
 * wrapper_armnn_tfl.hpp
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
 */

#ifndef WRAPPER_ARMNN_TFL_HPP_
#define WRAPPER_ARMNN_TFL_HPP_

#include <algorithm>
#include <functional>
#include <queue>
#include <memory>
#include <string>
#include <sys/time.h>
#include <vector>

#include <armnn/BackendId.hpp>
#include <armnn/BackendRegistry.hpp>
#include <armnn/IRuntime.hpp>
#include <armnn/Logging.hpp>
#include <armnnTfLiteParser/ITfLiteParser.hpp>

#define LOG(x) std::cerr

namespace wrapper_armnn_tfl {

	double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

	struct Config {
		std::string model_name;
		std::string labels_file_name;
		std::vector<armnn::BackendId> compute_devices;
		float input_mean = 127.5f;
		float input_std = 127.5f;
		int number_of_results = 5;
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

	class Tfl_Wrapper {
	private:
		armnnTfLiteParser::ITfLiteParserPtr              m_parser;
		armnn::NetworkId                                 m_networkId;
		armnn::IRuntimePtr                               m_runtime;
		std::vector<armnnTfLiteParser::BindingPointInfo> m_inputBindings;
		std::vector<armnn::TensorInfo>                   m_inputTensorInfos;
		std::vector<armnnTfLiteParser::BindingPointInfo> m_outputBindings;
		std::vector<armnn::TensorInfo>                   m_outputTensorInfos;
		bool                                             m_inputFloating;
		float                                            m_inputMean;
		float                                            m_inputStd;
		float                                            m_inferenceTime;
		int                                              m_numberOfResults;

	public:
		Tfl_Wrapper():
			m_parser(armnnTfLiteParser::ITfLiteParser::Create()),
			m_runtime(armnn::IRuntime::Create(armnn::IRuntime::CreationOptions())){}

		void Initialize(Config* conf)
		{
			m_inputFloating = false;
			m_inferenceTime = 0;
			m_inputMean = conf->input_mean;
			m_inputStd = conf->input_std;
			m_numberOfResults = conf->number_of_results;

			armnn::INetworkPtr network = m_parser->CreateNetworkFromBinaryFile(conf->model_name.c_str());
			if (!network) {
				throw armnn::Exception("Failed to create an ArmNN network");
			}

			armnn::IOptimizedNetworkPtr optimizedNet = armnn::Optimize(*network, conf->compute_devices, m_runtime->GetDeviceSpec());
			armnn::NetworkId m_networkId;
			m_runtime->LoadNetwork(m_networkId, std::move(optimizedNet));

			if (m_parser->GetSubgraphCount() != 1) {
				LOG(INFO) << "Model with more than 1 subgraph is not supported by this application\n";
				LOG(INFO) << "You can adapt the application to support this specific NN model.\n";
				exit(0);
			}

			size_t subgraphId = 0;
			std::vector<std::string> inputTensorNames = m_parser->GetSubgraphInputTensorNames(subgraphId);
			for (unsigned int i = 0; i < inputTensorNames.size() ; i++) {
				std::cout << "inputTensorNames[" << i << "]= " << inputTensorNames[i] << std::endl;
				armnnTfLiteParser::BindingPointInfo inputBinding = m_parser->GetNetworkInputBindingInfo(subgraphId, inputTensorNames[i]);
				armnn::TensorInfo inputTensorInfo = m_runtime->GetInputTensorInfo(m_networkId, inputBinding.first);
				m_inputBindings.push_back(inputBinding);
				m_inputTensorInfos.push_back(inputTensorInfo);
			}

			if (m_inputTensorInfos.at(0).GetDataType() == armnn::DataType::Float32) {
				m_inputFloating = true;
				LOG(INFO) << "Floating point Tensorflow Lite Model\n";
			}

			std::vector<std::string> outputTensorNames = m_parser->GetSubgraphOutputTensorNames(subgraphId);
			for (unsigned int i = 0; i < outputTensorNames.size() ; i++) {
				std::cout << "outputTensorNames[" << i << "]= " << outputTensorNames[i] << std::endl;
				armnnTfLiteParser::BindingPointInfo outputBinding = m_parser->GetNetworkOutputBindingInfo(subgraphId, outputTensorNames[i]);
				armnn::TensorInfo outputTensorInfo = m_runtime->GetOutputTensorInfo(m_networkId, outputBinding.first);
				m_outputBindings.push_back(outputBinding);
				m_outputTensorInfos.push_back(outputTensorInfo);
			}
		}

		bool IsModelQuantized()
		{
			return !m_inputFloating;
		}

		int GetInputWidth()
		{
			armnn::TensorShape shape;
			shape = m_inputTensorInfos[0].GetShape();
			return shape[2];
		}

		int GetInputHeight()
		{
			armnn::TensorShape shape;
			shape = m_inputTensorInfos[0].GetShape();
			return shape[2];
		}

		int GetInputChannels()
		{
			armnn::TensorShape shape;
			shape = m_inputTensorInfos[0].GetShape();
			return shape[3];
		}

		unsigned int GetNumberOfOutputs()
		{
			return m_outputTensorInfos.size();
		}

		unsigned int GetOutputSize(int index)
		{
			return m_outputTensorInfos.at(index).GetNumElements();
		}

		void RunInference(uint8_t* img, ObjDetect_Results* results)
		{
			if (m_inputFloating)
				RunInference<float>(img, results);
			else
				RunInference<uint8_t>(img, results);
		}

		template <class T>
		void RunInference(uint8_t* img, ObjDetect_Results* results)
		{
			int input_height = GetInputHeight();
			int input_width = GetInputWidth();
			int input_channels = GetInputChannels();
			auto sizeInBytes = input_height * input_width * input_channels;

			std::vector<T> in(sizeInBytes);
			if (m_inputFloating) {
				for (int i = 0; i < sizeInBytes; i++)
					in[i] = (img[i] - m_inputMean) / m_inputStd;
			} else {
				for (int i = 0; i < sizeInBytes; i++)
					in[i] = img[i];
			}

			armnn::InputTensors inputTensors;
			inputTensors.push_back({ m_inputBindings[0].first, armnn::ConstTensor(m_inputBindings[0].second, in.data()) });

			armnn::OutputTensors outputTensors;
			std::vector<std::vector<float>> out;
			for (unsigned int i = 0; i < GetNumberOfOutputs() ; i++) {
				std::vector<float> out_data(GetOutputSize(i));
				out.push_back(out_data);
				outputTensors.push_back({ m_outputBindings[i].first, armnn::Tensor(m_outputBindings[i].second, out[i].data()) });
			}

			struct timeval start_time, stop_time;
			gettimeofday(&start_time, nullptr);

			m_runtime->EnqueueWorkload(m_networkId, inputTensors, outputTensors);

			gettimeofday(&stop_time, nullptr);
			m_inferenceTime = (get_ms(stop_time) - get_ms(start_time));

			/* Get results */
			std::vector<float> locations = out[0];
			std::vector<float> classes = out[1];
			std::vector<float> scores = out[2];

			// get the output size by geting the size of the
			// output tensor 1 that represente the classes
			auto output_size = GetOutputSize(1);

			// the outputs are already sort by descending order
			for (unsigned int i = 0; i < output_size; i++) {
				results->score[i]       = scores[i];
				results->index[i]       = (int)classes[i];
				results->location[i].y0 = locations[(i * 4) + 0];
				results->location[i].x0 = locations[(i * 4) + 1];
				results->location[i].y1 = locations[(i * 4) + 2];
				results->location[i].x1 = locations[(i * 4) + 3];
			}
			results->inference_time = m_inferenceTime;
		}

		// Takes a file name, and loads a list of labels from it, one per line, and
		// returns a vector of the strings. It pads with empty strings so the length
		// of the result is a multiple of 16, because our model expects that.
		armnn::Status ReadLabelsFile(const std::string& file_name,
					     std::vector<std::string>* result,
					     size_t* found_label_count)
		{
			std::ifstream file(file_name);
			if (!file) {
				LOG(FATAL) << "Labels file " << file_name << " not found\n";
				return armnn::Status::Failure;
			}
			result->clear();
			std::string line;
			while (std::getline(file, line)) {
				result->push_back(line);
			}
			*found_label_count = result->size();
			const int padding = 16;
			while (result->size() % padding) {
				result->emplace_back();
			}
			return armnn::Status::Success;
		}
	};

}  // namespace wrapper_armnn_tfl

#endif  // WRAPPER_ARMNN_TFL_HPP_
