/*
 * libfacedetect_tfl.h
 *
 * Author: Vincent Abriou <vincent.abriou@st.com> for STMicroelectronics.
 *
 * Copyright (c) 2020 STMicroelectronics. All rights reserved.
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *
 *     http://www.st.com/SLA0044
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

#ifndef LIBFACEDETECT_TFL_H_
#define LIBFACEDETECT_TFL_H_

#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>
#include <memory>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/time.h>
#include <vector>

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"

#define LOG(x) std::cerr

struct Point {
	float x, y;
};

struct Bbox {
	Point top_left;
	Point bot_right;
	float score;
};

class Tfl_facedetect {
private:
	float m_inferenceTime;

public:
	Tfl_facedetect();
	void Initialize(int number_of_faces, int number_of_threads);
	float FindFaces(const cv::Mat& img, uint8_t* nb_faces);
	Bbox GetDetectedFaceBbox(uint32_t detected_face_idx);
	cv::Mat GetDetectedFaceImage(cv::Mat img, uint32_t detected_face_idx, cv::Size output_size);
};

#endif  // LIBFACEDETECT_TFL_H_
