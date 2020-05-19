/*
 * wrapper_armnn_tfl.hpp
 *
 * Copyright (C) 2020, STMicroelectronics - All Rights Reserved
 * Author: Vincent Abriou <vincent.abriou@st.com> for STMicroelectronics.
 *
 * License type: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef WRAPPER_ARMNN_TFL_HPP_
#define WRAPPER_ARMNN_TFL_HPP_

#include <algorithm>
#include <functional>
#include <queue>
#include <memory>
#include <numeric>
#include <string>
#include <sys/time.h>
#include <vector>

#include <armnn/BackendId.hpp>
#include <armnn/BackendRegistry.hpp>
#include <armnn/IRuntime.hpp>
#include <armnn/Logging.hpp>
#include <armnnTfLiteParser/ITfLiteParser.hpp>

namespace wrapper_armnn_tfl {

	double get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }
	double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

	struct Config {
		std::string model_name;
		std::vector<armnn::BackendId> compute_devices;
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
		std::vector<double>                              m_inferenceTimes;

	public:
		Tfl_Wrapper():
			m_parser(armnnTfLiteParser::ITfLiteParser::Create()),
			m_runtime(armnn::IRuntime::Create(armnn::IRuntime::CreationOptions())){}

		void Initialize(Config* conf)
		{
			m_inputFloating = false;

			armnn::INetworkPtr network = m_parser->CreateNetworkFromBinaryFile(conf->model_name.c_str());
			if (!network) {
				throw armnn::Exception("Failed to create an ArmNN network");
			}

			armnn::IOptimizedNetworkPtr optimizedNet = armnn::Optimize(*network, conf->compute_devices, m_runtime->GetDeviceSpec());
			armnn::NetworkId m_networkId;
			m_runtime->LoadNetwork(m_networkId, std::move(optimizedNet));

			if (m_parser->GetSubgraphCount() != 1) {
				std::cout << "Model with more than 1 subgraph is not supported by this application\n";
				std::cout << "You can adapt the application to support this specific NN model.\n";
				exit(0);
			}

			std::cout << "\nModel information:" << std::endl;
			size_t subgraphId = 0;
			std::vector<std::string> inputTensorNames = m_parser->GetSubgraphInputTensorNames(subgraphId);
			for (unsigned int i = 0; i < inputTensorNames.size() ; i++) {
				std::cout << "inputTensorNames[" << i << "] = " << inputTensorNames[i] << std::endl;
				armnnTfLiteParser::BindingPointInfo inputBinding = m_parser->GetNetworkInputBindingInfo(subgraphId, inputTensorNames[i]);
				armnn::TensorInfo inputTensorInfo = m_runtime->GetInputTensorInfo(m_networkId, inputBinding.first);
				m_inputBindings.push_back(inputBinding);
				m_inputTensorInfos.push_back(inputTensorInfo);
			}

			if (m_inputTensorInfos.at(0).GetDataType() == armnn::DataType::Float32) {
				m_inputFloating = true;
				std::cout << "Floating point Tensorflow Lite Model\n";
			}

			std::vector<std::string> outputTensorNames = m_parser->GetSubgraphOutputTensorNames(subgraphId);
			for (unsigned int i = 0; i < outputTensorNames.size() ; i++) {
				std::cout << "outputTensorNames[" << i << "] = " << outputTensorNames[i] << std::endl;
				armnnTfLiteParser::BindingPointInfo outputBinding = m_parser->GetNetworkOutputBindingInfo(subgraphId, outputTensorNames[i]);
				armnn::TensorInfo outputTensorInfo = m_runtime->GetOutputTensorInfo(m_networkId, outputBinding.first);
				m_outputBindings.push_back(outputBinding);
				m_outputTensorInfos.push_back(outputTensorInfo);
			}
		}

		unsigned int GetNumberOfInputs()
		{
			return m_inputTensorInfos.size();
		}

		unsigned int GetInputSize(int index)
		{
			return m_inputTensorInfos.at(index).GetNumElements();
		}

		unsigned int GetNumberOfOutputs()
		{
			return m_outputTensorInfos.size();
		}

		unsigned int GetOutputSize(int index)
		{
			return m_outputTensorInfos.at(index).GetNumElements();
		}

		void RunInference(unsigned int nb_loops)
		{
			if (m_inputFloating)
				RunInference<float>(nb_loops);
			else
				RunInference<uint8_t>(nb_loops);
		}

		template <class T>
		void RunInference(unsigned int nb_loops)
		{
			unsigned int nb_ouputs = GetNumberOfOutputs();
			unsigned int nb_inputs = GetNumberOfInputs();

			armnn::InputTensors inputTensors;
			std::vector<std::vector<T>> in;
			for (unsigned int i = 0 ; i < nb_inputs ; i++) {
				std::vector<T> in_data(GetInputSize(i));
				in.push_back(in_data);
				inputTensors.push_back({ m_inputBindings[i].first, armnn::ConstTensor(m_inputBindings[i].second, in.data()) });
			}

			armnn::OutputTensors outputTensors;
			std::vector<std::vector<float>> out;
			for (unsigned int i = 0; i < nb_ouputs ; i++) {
				std::vector<float> out_data(GetOutputSize(i));
				out.push_back(out_data);
				outputTensors.push_back({ m_outputBindings[i].first, armnn::Tensor(m_outputBindings[i].second, out[i].data()) });
			}

			std::cout << "\ninferences are running: " << std::flush;
			for (unsigned int i = 0 ; i < nb_loops ; i++) {
				struct timeval start_time, stop_time;
				gettimeofday(&start_time, nullptr);

				m_runtime->EnqueueWorkload(m_networkId, inputTensors, outputTensors);

				gettimeofday(&stop_time, nullptr);
				m_inferenceTimes.push_back((get_us(stop_time) - get_us(start_time)));
				std::cout << "# " << std::flush;
			}

			auto maxInfTime = *std::max_element(m_inferenceTimes.begin(), m_inferenceTimes.end());
			auto minInfTime = *std::min_element(m_inferenceTimes.begin(), m_inferenceTimes.end());
			auto avgInfTime = accumulate(m_inferenceTimes.begin(), m_inferenceTimes.end(), 0.0) / m_inferenceTimes.size();
			std::cout << "\n\ninference time: ";
			std::cout << "min=" << minInfTime << "us  ";
			std::cout << "max=" << maxInfTime << "us  ";
			std::cout << "avg=" << avgInfTime << "us" << std::endl;
		}
	};

}  // namespace wrapper_armnn_tfl

#endif  // WRAPPER_ARMNN_TFL_HPP_
