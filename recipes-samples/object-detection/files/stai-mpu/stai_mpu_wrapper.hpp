/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_WRAPPER_HPP_
#define STAI_MPU_WRAPPER_HPP_

#include <algorithm>
#include <functional>
#include <queue>
#include <memory>
#include <string>
#include <sys/time.h>
#include <vector>
#include <fstream>

#include "stai_mpu_network.h"

#define LOG(x) std::cerr

namespace wrapper_stai_mpu{

	/* Wrapper configuration structure */
	struct Config {
		bool verbose;
		float input_mean = 127.5f;
		float input_std = 127.5f;
		int number_of_threads = 2;
		int number_of_results = 5;
		std::string model_name;
		std::string labels_file_name;
	};

	/**
	 * Return time value in millisecond
	 */
	double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

	/* STAI Mpu Wrapper class */
	class stai_mpu_wrapper {
		private:
			std::vector<stai_mpu_tensor>                     m_input_infos;
			std::vector<int> 							 m_input_shape;
			std::vector<int> 							 m_output_shape;
			uint8_t* 									 m_input_tensor_int;
			float* 										 m_input_tensor_f;
			bool                                     	 m_verbose;
			bool                                     	 m_allow_fp16;
			float                                   	 m_inputMean;
			float                                  	 	 m_inputStd;
			int                                      	 m_numberOfThreads;
			int                                      	 m_numberOfResults;
			int 										 m_num_inputs;
			int 										 m_num_outputs;
			int 										 m_input_width;
			int 										 m_input_height;
			int 										 m_input_channels;
			int											 m_sizeInBytes;
			float                                   	 m_inferenceTime;

		public:
			std::unique_ptr<stai_mpu_network>             	 m_stai_mpu_model;
			std::vector<stai_mpu_tensor>					 m_output_infos;

			stai_mpu_wrapper() {}

		/* STAI Mpu Wrapper initialization */
		void Initialize(Config* conf)
		{
			m_allow_fp16 = false;
			m_inferenceTime = 0;
			m_verbose = conf->verbose;
			m_inputMean = conf->input_mean;
			m_inputStd = conf->input_std;
			m_numberOfThreads = conf->number_of_threads;
			m_numberOfResults = conf->number_of_results;

			if (!conf->model_name.c_str()) {
				LOG(ERROR) << "no model file name\n";
				exit(-1);
			}

			std::string model_path = conf->model_name.c_str();
			size_t dot_pos = model_path.find_last_of('.');
			// Depending on model extension enable or not hardware acceleration
			if (model_path.substr(dot_pos) == ".nb"){
				m_stai_mpu_model.reset(new stai_mpu_network(model_path, true));
			} else {
				m_stai_mpu_model.reset(new stai_mpu_network(model_path, false));
			}
			m_input_infos = m_stai_mpu_model->get_input_infos();
			m_output_infos = m_stai_mpu_model->get_output_infos();
			m_num_inputs = m_stai_mpu_model->get_num_inputs();
			m_num_outputs = m_stai_mpu_model->get_num_outputs();
			m_input_height = GetInputHeight();
			m_input_width = GetInputWidth();
			m_input_channels = GetInputChannels();
			m_sizeInBytes = m_input_height * m_input_width * m_input_channels;
			m_input_tensor_int = new uint8_t[m_sizeInBytes];
			m_input_tensor_f = new float[m_sizeInBytes];

		}

		/* Get the NN model inputs width */
		int GetInputWidth()
		{
			for (int i = 0; i < m_num_inputs; i++) {
				stai_mpu_tensor input_info = m_input_infos[i];
				m_input_shape = input_info.get_shape();
			}
			int input_width = m_input_shape[1];
			return input_width;
		}

		/* Get the NN model inputs height */
		int GetInputHeight()
		{
			for (int i = 0; i < m_num_inputs; i++) {
				stai_mpu_tensor input_info = m_input_infos[i];
				m_input_shape = input_info.get_shape();
			}
			int input_height = m_input_shape[2];
			return input_height;
		}

		/* Get the NN model inputs channels */
		int GetInputChannels()
		{
			for (int i = 0; i < m_num_inputs; i++) {
				stai_mpu_tensor input_info = m_input_infos[i];
				m_input_shape = input_info.get_shape();
			}
			int input_channels = m_input_shape[3];
			return input_channels;
		}

		/* Get the number of NN model inputs */
		int GetNumberOfInputs()
		{
			return m_num_inputs;
		}

		/* Get the number of NN model outputs */
		int GetNumberOfOutputs()
		{
			return m_num_outputs;
		}

		/* Get the shape of NN model outputs */
		std::vector<int> GetOutputShape(int index)
		{
			for (int i = 0; i < m_num_outputs; i++) {
				stai_mpu_tensor output_info = m_output_infos[i];
				m_output_shape = output_info.get_shape();
			}
			return m_output_shape;
		}

		/* Get the shape of NN model inputs */
		std::vector<int> GetInputShape(int index)
		{
			for (int i = 0; i < m_num_inputs; i++) {
				stai_mpu_tensor input_info = m_input_infos[i];
				m_input_shape = input_info.get_shape();
			}
			return m_input_shape;
		}

		/* Get the NN model inference time */
		float GetInferenceTime()
		{
			return m_inferenceTime;
		}

		/* Run NN model inference based on a picture */
		void RunInference(uint8_t* img)
		{
			bool floating_model = false;

			if (m_input_infos[0].get_dtype() == stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32){
					floating_model = true;
				}
			if (floating_model) {
				for (int i = 0; i < m_sizeInBytes; i++)
					m_input_tensor_f[i] = (img[i] - m_inputMean) / m_inputStd;
				m_stai_mpu_model->set_input(0, m_input_tensor_f);
			} else {
				for (int i = 0; i < m_sizeInBytes; i++)
					m_input_tensor_int[i] = img[i];
				m_stai_mpu_model->set_input(0, m_input_tensor_int);
			}

			struct timeval start_time, stop_time;
			gettimeofday(&start_time, nullptr);
			m_stai_mpu_model->run();
			gettimeofday(&stop_time, nullptr);
			m_inferenceTime = (get_ms(stop_time) - get_ms(start_time));
		}

	};
}  // namespace stai_mpu_wrapper

#endif  // STAI_MPU_WRAPPER_HPP_
