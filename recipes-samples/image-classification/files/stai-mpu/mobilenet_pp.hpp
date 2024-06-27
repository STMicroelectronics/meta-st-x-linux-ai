/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef MOBILENET_PP_HPP_
#define MOBILENET_PP_HPP_

#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include "stai_mpu_network.h"

#define LOG(x) std::cerr

namespace nn_postproc{

	bool nn_post_proc_first_call = true;

	struct Label_Results {
		float accuracy[10];
		int index[10];
		float inference_time;
	};

	// This function is used to process the ouput of the model and recover relevant information such as class detected and
	// associated accuracy. The output tensor of the model is recover through the model structure. A structure named Results
	// is populated with theses information to be used is the application core.
	void nn_post_proc(std::unique_ptr<stai_mpu_network>& nn_model,std::vector<stai_mpu_tensor> output_infos, Label_Results* results)
	{
		void* outputs_tensor = nn_model->get_output(0);
		int output_dims = output_infos[0].get_rank();
		stai_mpu_dtype output_dtype = output_infos[0].get_dtype();
		stai_mpu_backend_engine stai_backend = nn_model->get_backend_engine();

		/* Get output shape */
		std::vector<int> output_shape = output_infos[0].get_shape();

		/* Get output size */
		unsigned int output_size  = output_shape[output_dims-1];

		/* Process output data depending on the date type FLOAT32/16 or UINT8/INT8 */
		if (output_dtype == stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32 || output_dtype == stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT16) {
			float* output_data = static_cast<float*>(outputs_tensor);
			for (int i = 0; i < 5; i++) {
				results->index[i] = std::distance(&output_data[0], std::max_element(&output_data[0], &output_data[output_size]));
				results->accuracy[i] = output_data[results->index[i]];
				output_data[results->index[i]] = 0;
			}
		} else {
			uint8_t* output_data = static_cast<uint8_t*>(outputs_tensor);
			for (int i = 0; i < 5; i++) {
				results->index[i] = std::distance(&output_data[0], std::max_element(&output_data[0], &output_data[output_size]));
				results->accuracy[i] = output_data[results->index[i]] / 255.0;
				output_data[results->index[i]] = 0;
			}
		}
		/* Release the output vector to avoid memory leak issues */
		if (stai_backend == stai_mpu_backend_engine::STAI_MPU_OVX_NPU_ENGINE){
			free(outputs_tensor);
		}
	};

	// Takes a file name, and loads a list of labels from it, one per line, and
	// returns a vector of the strings. It pads with empty strings so the length
	// of the result is a multiple of 16, because our model expects that.
	int ReadLabelsFile(const std::string& file_name,
					std::vector<std::string>* result,
					size_t* found_label_count)
	{
		std::ifstream file(file_name);
		if (!file) {
			LOG(FATAL) << "Labels file " << file_name << " not found\n";
			return 1;
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
		return 0;
	};

}  // namespace nn_postproc

#endif  // MOBILENET_PP_HPP_
