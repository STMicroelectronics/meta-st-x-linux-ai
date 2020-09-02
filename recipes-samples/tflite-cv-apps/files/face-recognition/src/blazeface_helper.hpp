/*
 * blazeface_helper.hpp
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
 * The way to compute the anchors have been inspired from:
 * https://github.com/google/mediapipe/blob/master/mediapipe/calculators/tflite/ssd_anchors_calculator.cc
 * Copyright 2019 The MediaPipe Authors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef BLAZEFACE_HELPER_HPP_
#define BLAZEFACE_HELPER_HPP_

#include <math.h>
#include <opencv2/opencv.hpp>

#define LOG(x) std::cerr

namespace blazeface_helper {

	struct Rect {
		float x0, y0, x1, y1;
	};

	struct Point {
		float x, y;
	};

	struct Face_Detection {
		float score;
		Rect face;
		float area;
		Point eye_l, eye_r;
		Point noze;
		Point mouth;
	};

	struct Face_Results {
		std::vector<Face_Detection> detections;
	};

	class BlazeFace {
	private:
		struct Anchor {
			// anchor box center.
			float x_center;
			float y_center;
			// anchor box height.
			float h;
			// anchor box width.
			float w;
		};

		#define NUM_OF_BOXES             896
		#define NUM_COORDS               16
		#define MIN_SCORE_THRESH         0.80f
		#define X_SCALE                  128.0f
		#define Y_SCALE                  128.0f
		#define H_SCALE                  128.0f
		#define W_SCALE                  128.0f
		#define MIN_SIMILARITY_THRESHOLD 0.5f

		std::vector<Anchor> m_anchors;
		bool m_anchorCalculDone = false;

		//anchor calculator parameters
		const std::array<int, 4>  m_strides = {8, 16, 16, 16};
		const float               m_minScale = 0.1484375;
		const float               m_maxScale = 0.75;
		const int                 m_inputSizeWidth = 128;
		const int                 m_inputSizeHeight = 128;
		const float               m_anchorOffsetX = 0.5;
		const float               m_anchorOffsetY = 0.5;
		const std::array<float,1> m_aspectRatios = {1.0};

		static float CalculateScale(float min_scale,
					    float max_scale,
					    int stride_index, int num_strides)
		{
			if (num_strides == 1) {
				return (min_scale + max_scale) * 0.5f;
			} else {
				return min_scale +
					(max_scale - min_scale) * 1.0 * stride_index / (num_strides - 1.0f);
			}
		}

		static bool CompareScore(const Face_Detection& a,
					 const Face_Detection& b)
		{
			// biggest comes first
			return a.score > b.score;
		}

		static bool CompareArea(const Face_Detection& a,
					const Face_Detection& b)
		{
			// biggest comes first
			return a.area > b.area;
		}

		static bool BoxSimilarity(Face_Detection *a,
					  Face_Detection *b)
		{
			cv::Rect_<float> box_a;
			cv::Rect_<float> box_b;

			box_a.x = a->face.x0;
			box_a.y = a->face.y0;
			box_a.width = a->face.x1 - a->face.x0;
			box_a.height = a->face.y1 - a->face.y0;
			a->area = box_a.width * box_a.height;

			box_b.x = b->face.x0;
			box_b.y = b->face.y0;
			box_b.width = b->face.x1 - b->face.x0;
			box_b.height = b->face.y1 - b->face.y0;
			b->area = box_b.width * box_b.height;

			cv::Rect_<float> box_intersect = box_a & box_b;

			float intersect_area = box_intersect.width * box_intersect.height;
			float norm = a->area + b->area - intersect_area;

			float overlap_ratio = norm > 0.0f ? intersect_area / norm : 0.0f;

			if (overlap_ratio > MIN_SIMILARITY_THRESHOLD)
				return true;

			return false;
		}


	public:
		TfLiteStatus CalculateAnchors() {
			unsigned int layer_id = 0;
			while (layer_id < m_strides.size()) {
				std::vector<float> anchor_height;
				std::vector<float> anchor_width;
				std::vector<float> aspect_ratios;
				std::vector<float> scales;

				// For same strides, we merge the anchors in the same order.
				unsigned int last_same_stride_layer = layer_id;
				while (last_same_stride_layer < m_strides.size() &&
				       m_strides[last_same_stride_layer] == m_strides[layer_id]) {
					const float scale =
						CalculateScale(m_minScale, m_maxScale,
							       last_same_stride_layer,
							       m_strides.size());
					for (unsigned int aspect_ratio_id = 0; aspect_ratio_id < m_aspectRatios.size(); ++aspect_ratio_id) {
						aspect_ratios.push_back(m_aspectRatios[aspect_ratio_id]);
						scales.push_back(scale);
					}
					const float scale_next =
						last_same_stride_layer == m_strides.size() - 1
						? 1.0f
						: CalculateScale(m_minScale, m_maxScale,
								 last_same_stride_layer + 1,
								 m_strides.size());
					scales.push_back(std::sqrt(scale * scale_next));
					aspect_ratios.push_back(1.0f);
					last_same_stride_layer++;
				}

				for (unsigned int i = 0; i < aspect_ratios.size(); ++i) {
					const float ratio_sqrts = std::sqrt(aspect_ratios[i]);
					anchor_height.push_back(scales[i] / ratio_sqrts);
					anchor_width.push_back(scales[i] * ratio_sqrts);
				}

				const int stride = m_strides[layer_id];
				int feature_map_width = std::ceil(1.0f * m_inputSizeWidth / stride);
				int feature_map_height = std::ceil(1.0f * m_inputSizeHeight / stride);

				for (int y = 0; y < feature_map_height; ++y) {
					for (int x = 0; x < feature_map_width; ++x) {
						for (unsigned int anchor_id = 0; anchor_id < anchor_height.size(); ++anchor_id) {
							const float x_center = (x + m_anchorOffsetX) * 1.0f / feature_map_width;
							const float y_center = (y + m_anchorOffsetY) * 1.0f / feature_map_height;

							Anchor new_anchor;
							new_anchor.x_center = x_center;
							new_anchor.y_center = y_center;
							new_anchor.w =1.0f;
							new_anchor.h = 1.0f;

							m_anchors.push_back(new_anchor);
						}
					}
				}
				layer_id = last_same_stride_layer;
			}
			m_anchorCalculDone = true;
			return kTfLiteOk;
		}

		int GetNumOfBoxes() {
			return (NUM_OF_BOXES);
		}

		float GetAnchorWidth(int index) {
			if (index < NUM_OF_BOXES)
				return m_anchors.at(index).w;
			LOG(ERROR) << "index is greater that the number of boxes (" << NUM_OF_BOXES << ")\n";
			return -1.0;
		}

		float GetAnchorHeight(int index) {
			if (index < NUM_OF_BOXES)
				return m_anchors.at(index).h;
			LOG(ERROR) << "index is greater that the number of boxes (" << NUM_OF_BOXES << ")\n";
			return -1.0;
		}

		float GetAnchorCenterX(int index) {
			if (index < NUM_OF_BOXES)
				return m_anchors.at(index).x_center;
			LOG(ERROR) << "index is greater that the number of boxes (" << NUM_OF_BOXES << ")\n";
			return -1.0;
		}

		float GetAnchorCenterY(int index) {
			if (index < NUM_OF_BOXES)
				return m_anchors.at(index).y_center;
			LOG(ERROR) << "index is greater that the number of boxes (" << NUM_OF_BOXES << ")\n";
			return -1.0;
		}

		void GetDetectedFaceLandmarks(float *classificator,
					      float* regressors,
					      int max_faces,
					      Face_Results* results)
		{
			/* clear content of the previous detection */
			results->detections.clear();

			if (!m_anchorCalculDone) {
				LOG(ERROR) << "Anchor calcul not done!\n";
				return;
			}

			for (unsigned int i = 0; i < NUM_OF_BOXES; i++) {
				float score = classificator[i];
				score = score < -100.0f ? -100.0f : score;
				score = score > 100.0f ? 100.0f : score;
				score = 1.0f / (1.0f + (float) exp(-score));

				if (score <= MIN_SCORE_THRESH)
					continue;

				float face_x_center = regressors[i * NUM_COORDS];
				float face_y_center = regressors[i * NUM_COORDS + 1];
				float face_w = regressors[i * NUM_COORDS + 2];
				float face_h = regressors[i * NUM_COORDS + 3];
				face_x_center = face_x_center / X_SCALE * GetAnchorWidth(i) + GetAnchorCenterX(i);
				face_y_center = face_y_center / Y_SCALE * GetAnchorHeight(i) + GetAnchorCenterY(i);
				face_w = face_w / W_SCALE * GetAnchorWidth(i);
				face_h = face_h / H_SCALE * GetAnchorHeight(i);

				float eye_l_x = regressors[i * NUM_COORDS + 4];
				float eye_l_y = regressors[i * NUM_COORDS + 5];
				eye_l_x = eye_l_x / X_SCALE * GetAnchorWidth(i) + GetAnchorCenterX(i);
				eye_l_y = eye_l_y / Y_SCALE * GetAnchorHeight(i) + GetAnchorCenterY(i);

				float eye_r_x = regressors[i * NUM_COORDS + 6];
				float eye_r_y = regressors[i * NUM_COORDS + 7];
				eye_r_x = eye_r_x / X_SCALE * GetAnchorWidth(i) + GetAnchorCenterX(i);
				eye_r_y = eye_r_y / Y_SCALE * GetAnchorHeight(i) + GetAnchorCenterY(i);

				float noze_x = regressors[i * NUM_COORDS + 8];
				float noze_y = regressors[i * NUM_COORDS + 9];
				noze_x = noze_x / X_SCALE * GetAnchorWidth(i) + GetAnchorCenterX(i);
				noze_y = noze_y / Y_SCALE * GetAnchorHeight(i) + GetAnchorCenterY(i);

				float mouth_x = regressors[i * NUM_COORDS + 10];
				float mouth_y = regressors[i * NUM_COORDS + 11];
				mouth_x = mouth_x / X_SCALE * GetAnchorWidth(i) + GetAnchorCenterX(i);
				mouth_y = mouth_y / Y_SCALE * GetAnchorHeight(i) + GetAnchorCenterY(i);

				Face_Detection new_detection;
				new_detection.score = score;
				new_detection.face.x0 = face_x_center - face_w / 2.f;
				new_detection.face.y0 = face_y_center - face_h / 2.f;
				new_detection.face.x1 = face_x_center + face_w / 2.f;
				new_detection.face.y1 = face_y_center + face_h / 2.f;
				new_detection.eye_l.x = eye_l_x;
				new_detection.eye_l.y = eye_l_y;
				new_detection.eye_r.x = eye_r_x;
				new_detection.eye_r.y = eye_r_y;
				new_detection.noze.x = noze_x;
				new_detection.noze.y = noze_y;
				new_detection.mouth.x = mouth_x;
				new_detection.mouth.y = mouth_y;

				results->detections.push_back(new_detection);
			}

			// If no detection, return the empty results
                        if (results->detections.empty())
				return;

			// sort the detected face by descending scores
			std::sort(results->detections.begin(),
				  results->detections.end(),
				  CompareScore);

			// revome similar boxes
			unsigned int index = 0;
			while (index < results->detections.size()) {
				unsigned int i = index + 1;
				while (i < results->detections.size()) {
					if (BoxSimilarity(&results->detections.at(index),
							  &results->detections.at(i)))
						results->detections.erase(results->detections.begin() + i);
					else
						i++;
				}
				index++;
			}

			// remove all detected faces with bounding box that
			// exceed the limit of the frame
			index = 0;
			while (index < results->detections.size()) {
				if ((results->detections.at(index).face.x0 < 0.0) ||
				    (results->detections.at(index).face.x0 > 1.0) ||
				    (results->detections.at(index).face.y0 < 0.0) ||
				    (results->detections.at(index).face.y0 > 1.0) ||
				    (results->detections.at(index).face.x1 < 0.0) ||
				    (results->detections.at(index).face.x1 > 1.0) ||
				    (results->detections.at(index).face.y0 < 0.0) ||
				    (results->detections.at(index).face.y1 > 1.0))
					results->detections.erase(results->detections.begin() + index);
				else
					index++;
			}

			// sort the detected face by descending box area
			std::sort(results->detections.begin(), results->detections.end(),
				  CompareArea);

			// finally clip the number face to the max_faces value
			if ((int)results->detections.size() > max_faces)
				results->detections.erase(results->detections.begin() + max_faces,
							  results->detections.end());
			return;
		}
	};

}  // namespace wrapper_tfl

#endif  // WRAPPER_TFL_HPP_
