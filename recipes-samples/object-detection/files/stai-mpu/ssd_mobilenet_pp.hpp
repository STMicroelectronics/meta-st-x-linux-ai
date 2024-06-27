/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef SSD_MOBILENET_PP_HPP_
#define SSD_MOBILENET_PP_HPP_

#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include "stai_mpu_network.h"

#define LOG(x) std::cerr

namespace nn_postproc{

	bool nn_post_proc_first_call = true;

	/* Structure used for box coordinates */
	struct ObjDetect_Location {
		float y0, x0, y1, x1;
	};

	/* Structure used to store bounding box information : class, score, box coordinates */
	struct ObjDetect_Results {
		int class_index;
		float score;
		ObjDetect_Location location;
	};

	/* Structure used to store frame inference result: inference time, vector of ObjDetect_Results */
	struct Frame_Results {
        std::vector<ObjDetect_Results> vect_ObjDetect_Results;
		float inference_time;
		stai_mpu_backend_engine ai_backend;
		std::string model_type;
	};

	/**
	 * Return time value in millisecond
	 */
	double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

	/**
	 * Function used to filter the raw NN output by score
	 * Each results that are not over the confidence threshold are dropped
	 * Return the vector of box index that are over the threshold
	 */
	std::vector<int> Filter_by_score(float* predictions, int rows, int cols, float confidence_thresh) {
	std::vector<int> filtered_indexes;
	for (int i = 0; i < rows; ++i) {
		bool value_over_threshold = false;
		for (int j = 1; j < cols; ++j) {  // Start from column 1 as per your requirement
			if (predictions[i * cols + j] > confidence_thresh) {
				value_over_threshold = true;
				break;  // No need to check other values in the same row
			}
		}
		if (value_over_threshold) {
			filtered_indexes.push_back(i);  // Store the row index
		}
	}
	return filtered_indexes;
	}

	/**
	 * Function used to decode raw NN output using associated anchors
	 * Return a float vector of decoded outputs
	 */
	std::vector<float> BB_decoding(std::vector<float> encoded_bbox, std::vector<float> anchors){
		std::vector<float> decoded_boxes(encoded_bbox.size());
		int it = int(encoded_bbox.size())/4;
		for (int i=0; i<it; i++){
			float a_xmin = anchors[i * 4];
			float a_ymin = anchors[i * 4 + 1];
			float a_xmax = anchors[i * 4 + 2];
			float a_ymax = anchors[i * 4 + 3];

			float bb_xmin = encoded_bbox[i * 4];
			float bb_ymin = encoded_bbox[i * 4 + 1];
			float bb_xmax = encoded_bbox[i * 4 + 2];
			float bb_ymax = encoded_bbox[i * 4 + 3];

			float w = a_xmax - a_xmin;
			float h = a_ymax - a_ymin;

			float decoded_xmin = bb_xmin * w;
			float decoded_ymin = bb_ymin * h;
			float decoded_xmax = bb_xmax * w;
			float decoded_ymax = bb_ymax * h;

			decoded_xmin += a_xmin;
			decoded_ymin += a_ymin;
			decoded_xmax += a_xmax;
			decoded_ymax += a_ymax;
			decoded_boxes[i * 4] = decoded_xmin;
       		decoded_boxes[i * 4 + 1] = decoded_ymin;
        	decoded_boxes[i * 4 + 2] = decoded_xmax;
        	decoded_boxes[i * 4 + 3] = decoded_ymax;
		}
		return decoded_boxes;
	}

	/**
	 * Function used to calculate intersection over union of two boxes
	 * Used in the Non Max Suppression process
	 * Return IOU of the two boxes
	 */
	float IoU(const ObjDetect_Location& a, const ObjDetect_Location& b) {
		float areaA = (a.x1 - a.x0) * (a.y1 - a.y0);
		if (areaA <= 0.0f) return 0.0f;

		float areaB = (b.x1 - b.x0) * (b.y1 - b.y0);
		if (areaB <= 0.0f) return 0.0f;

		float intersectionMinX = std::max(a.x0, b.x0);
		float intersectionMaxX = std::min(a.x1, b.x1);
		float intersectionMinY = std::max(a.y0, b.y0);
		float intersectionMaxY = std::min(a.y1, b.y1);

		float intersectionArea = std::max(0.0f, intersectionMaxX - intersectionMinX) *
								std::max(0.0f, intersectionMaxY - intersectionMinY);

		return intersectionArea / (areaA + areaB - intersectionArea);
	}

	/**
	 * Function used to reproduce Non Max Supression technique
	 * This technique is used to filter boxes that are redondant
	 * or with too much overlap
	 * Return the final vector of ObjDetect_Results
	 */
	std::vector<ObjDetect_Results> non_max_suppression(const std::vector<float>& bb_predictions, std::vector<int>& class_index, std::vector<float>& filtered_scores,float iou_threshold) {
		const size_t box_size = 4;
		size_t num_boxes = bb_predictions.size() / box_size;
		std::vector<ObjDetect_Results> boxes(num_boxes);
		std::vector<int>::iterator result;
		// Convert the flat vector to BoxInfo structs
		for (size_t i = 0; i < num_boxes; ++i) {
			boxes[i].location.x0 = bb_predictions[i * box_size];
			boxes[i].location.y0 = bb_predictions[i * box_size + 1];
			boxes[i].location.x1 = bb_predictions[i * box_size + 2];
			boxes[i].location.y1 = bb_predictions[i * box_size + 3];
			boxes[i].score = filtered_scores[i];
			boxes[i].class_index = class_index[i];
		}

		// Sort boxes by score in descending order
		std::vector<int> indices(num_boxes);
		std::iota(indices.begin(), indices.end(), 0);
		std::sort(indices.begin(), indices.end(), [&boxes](int a, int b) {
			return boxes[a].score > boxes[b].score;
		});

		std::vector<bool> suppressed(num_boxes, false);
		std::vector<ObjDetect_Results> final_output;
		ObjDetect_Results bb_keeped;

		// Filter boxes to keep and to remove based on IOU
		for (size_t i = 0; i < num_boxes; ++i) {
			if (suppressed[indices[i]]) {
				continue;
			}

			int idx = indices[i];
			bb_keeped.location.x0 = boxes[idx].location.x0;
			bb_keeped.location.y0 = boxes[idx].location.y0;
			bb_keeped.location.x1 = boxes[idx].location.x1;
			bb_keeped.location.y1 = boxes[idx].location.y1;
			bb_keeped.score = boxes[idx].score;
			bb_keeped.class_index = boxes[idx].class_index;
			final_output.push_back(bb_keeped);
			for (size_t j = i + 1; j < num_boxes; ++j) {
				if (!suppressed[indices[j]] && IoU(boxes[idx].location, boxes[indices[j]].location) > iou_threshold) {
					suppressed[indices[j]] = true;
				}
			}
		}
		return final_output;
	}

	/**
	 * Function used to extract from the class prediction output relevant information such as
	 * highest score and class index
	 */
	void recover_score_info(const std::vector<float>& scores, int number_of_boxes, int number_of_classes,
                                 std::vector<float>& highest_scores, std::vector<int>& class_indices){
		for (int box = 0; box < number_of_boxes; ++box) {
			int start_index = box * number_of_classes;
			auto max_it = std::max_element(scores.begin() + start_index + 1, scores.begin() + start_index + number_of_classes);
			highest_scores.push_back(*max_it);
			class_indices.push_back(std::distance(scores.begin() + start_index, max_it));
		}
	}


	/**
	 * NN post processing :
	 * Get NN inference outputs
	 * Decode
	 * Filter
	 * Populate Frame result structure for drawing phase
	 */
	void nn_post_proc(std::unique_ptr<stai_mpu_network>& nn_model,std::vector<stai_mpu_tensor> output_infos, Frame_Results* results, float confidenceThresh, float iou_threshold, std::string model_type)
	{
		std::string ssd_mobilenet_v1_type = "ssd_mobilenet_v1";
		std::string ssd_mobilenet_v2_type = "ssd_mobilenet_v2";

		if (model_type == ssd_mobilenet_v2_type){

			/* Get output size */
			std::vector<int> output_shape_0 = output_infos[0].get_shape();
			std::vector<int> output_shape_1 = output_infos[1].get_shape();
			int number_of_boxes = output_shape_0[1];
			int number_of_classes = output_shape_0[2];
			int number_of_coordinates = output_shape_1[2];

			/* Get backend used */
			results->ai_backend = nn_model->get_backend_engine();

			/* Get inference outputs */
			float* box_encoded = static_cast<float*>(nn_model->get_output(1));
			float* class_prediction = static_cast<float*>(nn_model->get_output(0));
			float* anchors = static_cast<float*>(nn_model->get_output(2));

			/* First filtering by score */
			std::vector<int> filtered_indexes = Filter_by_score(class_prediction, number_of_boxes, number_of_classes, confidenceThresh);
			std::vector<float>  filtered_encoded_bb(filtered_indexes.size()*number_of_coordinates);
			std::vector<float>  filtered_anchors(filtered_indexes.size()*number_of_coordinates);
			std::vector<float>  filtered_class_prediction(filtered_indexes.size()*number_of_classes);

			// Copy the filtered elements into the new vectors of bb and anchors
			for (size_t i = 0; i < filtered_indexes.size(); ++i) {
				int row_index = filtered_indexes[i];
				for (int j = 0; j < number_of_coordinates; ++j) {
					filtered_encoded_bb[i * number_of_coordinates + j] = box_encoded[row_index * number_of_coordinates + j];
					filtered_anchors[i * number_of_coordinates + j] = anchors[row_index * number_of_coordinates + j];
				}
			}

			// Copy the filtered elements into the new vectors of bb and anchors
			for (size_t i = 0; i < filtered_indexes.size(); ++i) {
				int row_index = filtered_indexes[i];
				for (int j = 0; j < number_of_classes; ++j) {
					filtered_class_prediction[i * number_of_classes + j] = class_prediction[row_index * number_of_classes + j];
				}
			}

			/* Decode raw output of the NN model */
			std::vector<float> decoded_bb = BB_decoding(filtered_encoded_bb, filtered_anchors);

			/* Reformat output */
			std::vector<float> score;
			std::vector<int> class_index;
			recover_score_info(filtered_class_prediction,filtered_indexes.size(),number_of_classes,score,class_index);

			/* Apply NMS based filtering */
			results->vect_ObjDetect_Results = non_max_suppression(decoded_bb,class_index,score,iou_threshold);

			/* Release memory */
			if (results->ai_backend == stai_mpu_backend_engine::STAI_MPU_OVX_NPU_ENGINE){
				free(box_encoded);
				free(class_prediction);
				free(anchors);
			}

		} else if (model_type == ssd_mobilenet_v1_type){

			float *locations = static_cast<float*>(nn_model->get_output(0));
			float *classes = static_cast<float*>(nn_model->get_output(1));
			float *scores = static_cast<float*>(nn_model->get_output(2));

			/* Get output size */
			std::vector<int> output_shape = output_infos[1].get_shape();
			unsigned int output_size  = output_shape[output_shape.size()-1];

			// creation of an ObjDetect_Results struct to store values
			// of detected object the frame
			ObjDetect_Results Obj_detected;

			// the outputs are already sort by descending order
			if (nn_post_proc_first_call){
				for (unsigned int i = 0; i < output_size; i++) {
					Obj_detected.class_index =(int)classes[i];
					Obj_detected.score = scores[i];
					Obj_detected.location.y0 = locations[(i * 4) + 0];
					Obj_detected.location.x0 = locations[(i * 4) + 1];
					Obj_detected.location.y1 = locations[(i * 4) + 2];
					Obj_detected.location.x1 = locations[(i * 4) + 3];
					results->vect_ObjDetect_Results.push_back(Obj_detected);
				}
			} else {
				for (unsigned int i = 0; i < output_size; i++) {
					results->vect_ObjDetect_Results.erase(results->vect_ObjDetect_Results.begin()+i);
					Obj_detected.class_index =(int)classes[i];
					Obj_detected.score = scores[i];
					Obj_detected.location.y0 = locations[(i * 4) + 0];
					Obj_detected.location.x0 = locations[(i * 4) + 1];
					Obj_detected.location.y1 = locations[(i * 4) + 2];
					Obj_detected.location.x1 = locations[(i * 4) + 3];
					results->vect_ObjDetect_Results.insert(results->vect_ObjDetect_Results.begin()+i,Obj_detected);
				}
			}
			nn_post_proc_first_call = false;
		}

	}

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

#endif  // SSD_MOBILENET_PP_HPP_
