/*
 * libfacereco_tfl.h
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

#ifndef LIBFACERECO_TFL_H_
#define LIBFACERECO_TFL_H_

#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>
#include <memory>
#include <string>
#include <sys/time.h>
#include <vector>

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"

#define LOG(x) std::cerr

class Tfl_facereco {
private:
	float m_inferenceTime;

public:
	Tfl_facereco();
	void Initialize(int number_of_threads);
	void DisplaySettings();
	void DisplayModelInformation();
	bool IsModelQuantized();
	int GetInputWidth();
	int GetInputHeight();
	int GetInputChannels();
	unsigned int GetOutputSize(int index);
	float* GetOutputTensor(unsigned int index);
	float RunInference(uint8_t* img);
};

#endif  // LIBFACERECO_TFL_H_
