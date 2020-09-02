/* Copyright (C) 2019 NXP Semiconductors
* See LICENSE file in the project root for full license information.
* Author: 
* Xiyue Shi xiyue_shi@163.com
* Devin Jiao bin.jiao@nxp.com
* Any questions, please contact with bin.jiao@nxp.com
* 19/06/2019
*
* SPDX-License-Identifier:    Apache-2.0
*
*/

#ifndef FACE_DETECT_HELPERS_H_
#define FACE_DETECT_HELPERS_H_

#include <iostream>

#include "opencv2/objdetect.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

#include "face_recognition.h"
#include "face_detect_helpers_impl.h"

using namespace cv;

namespace tflite
{
namespace facerec
{

void init_face_detect(Settings *setting);
void init_window();
Mat &get_display();

void show_matched_face(Mat &face, String &label, float similarity);
void no_matched_face();
Mat get_data_face(Mat &img);
Mat get_test_face();

bool is_pause();

template <class T>
void copy_resize(T *out, Mat &in, int wanted_height, int wanted_width, int wanted_channels, bool is_float);

template void copy_resize<uint8_t>(uint8_t *, Mat &, int, int, int, bool);
template void copy_resize<float>(float *, Mat &, int, int, int, bool);

} // namespace facerec
} // namespace tflite

#endif
