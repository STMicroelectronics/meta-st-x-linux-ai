/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef FACENET_PP_HPP_
#define FACENET_PP_HPP_

#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <math.h>
#include <cmath>
#include <iostream>
#include <semaphore.h>
#include <opencv2/opencv.hpp>
#include "stai_mpu_network.h"

#define LOG(x) std::cerr

namespace nn_postproc_fr{
	bool nn_post_proc_first_call = true;

	struct inference_Results {
		float nn_output[512];
		float inference_time;
		stai_mpu_backend_engine ai_backend;
	};

	void save_float_array(const char* filename, const float* array, size_t size) {
		std::ofstream file(filename, std::ios::binary);
		if (!file) {
			std::cerr << "Error opening file for writing: " << filename << std::endl;
			return;
		}
		file.write(reinterpret_cast<const char*>(array), size * sizeof(float));
		file.close();
	}

	void load_float_array(const char* filename, float* array, size_t size) {
		std::ifstream file(filename, std::ios::binary);
		if (!file) {
			std::cerr << "Error opening file for reading: " << filename << std::endl;
			return;
		}
		file.read(reinterpret_cast<char*>(array), size * sizeof(float));
		file.close();
	}

	// Function to calculate the dot product of two float arrays
	float dot_product(const float* a, const float* b, size_t size) {
		float dot = 0.0f;
		for (size_t i = 0; i < size; ++i) {
			dot += a[i] * b[i];
		}
		return dot;
	}

	// Function to calculate the norm of a float array
	float norm(const float* vec, size_t size) {
		float sum = 0.0f;
		for (size_t i = 0; i < size; ++i) {
			sum += vec[i] * vec[i];
		}
		return std::sqrt(sum);
	}

	// Function to calculate cosine similarity
	float cosine_similarity(const float* a, const float* b, size_t size) {
		float dot = dot_product(a, b, size);
		float norm_a = norm(a, size);
		float norm_b = norm(b, size);

		// Check for division by zero
		if (norm_a == 0.0f || norm_b == 0.0f) {
			std::cerr << "Error: One of the vectors has zero norm." << std::endl;
			return 0.0f; // or some other error value
		}

		return 1.0f - (dot / (norm_a * norm_b));
	}

	// This function is used to process the ouput of the model and recover relevant information such as class detected and
	// associated accuracy. The output tensor of the model is recover through the model structure. A structure named Results
	// is populated with theses information to be used is the application core.
	void nn_post_proc(std::unique_ptr<stai_mpu_network>& nn_model,std::vector<stai_mpu_tensor> output_infos, inference_Results* results)
	{
		/* Get output size */
		std::vector<int> output_shape_0 = output_infos[0].get_shape();
		stai_mpu_quant_params qparams_output0 =  output_infos[0].get_qparams();
		float scale_o0 = qparams_output0.static_affine.scale;
		int zero_point_o0 = qparams_output0.static_affine.zero_point;
		results->ai_backend = nn_model->get_backend_engine();

		/* Get inference outputs */
		uint8_t *outputs = static_cast<uint8_t*>(nn_model->get_output(0));
		int i;
		for (i = 0; i < 512; i++) {
			results->nn_output[i] = (outputs[i]-zero_point_o0) * scale_o0;
		}

		/* Release memory */
		if (results->ai_backend == stai_mpu_backend_engine::STAI_MPU_OVX_NPU_ENGINE){
			free(outputs);
		}
	};
}  // namespace nn_postproc_fr

#endif  // FACENET_PP_HPP_
