/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================
* Copyright (C) 2019 NXP Semiconductors
* Author: 
* Xiyue Shi xiyue_shi@163.com
* Devin Jiao bin.jiao@nxp.com
* Any questions, please contact with bin.jiao@nxp.com
* 19/06/2019
*
* Add OpenCV support. 
*
* References:
* https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/examples/label_image/bitmap_helpers_impl.h
*/

#ifndef FACE_DETECT_HELPERS_IMPL_H_
#define FACE_DETECT_HELPERS_IMPL_H_

#include "opencv2/imgproc.hpp"

#include "tensorflow/lite/builtin_op_data.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/string_util.h"

#define LOG(x) std::cerr

namespace tflite {
namespace facerec {

template <class T>
void copy_resize(T *out, Mat &in, int wanted_height, int wanted_width, int wanted_channels, bool is_float)
{
  if (!in.isContinuous())
  {
    LOG(FATAL) << "image data not continuous"
               << "\n\r";
    exit(-1);
  }
  int image_height = in.size().height;
  int image_width = in.size().width;
  int image_channels = in.channels();
  int number_of_pixels = image_height * image_width * image_channels;
  std::unique_ptr<Interpreter> interpreter(new Interpreter);

  int base_index = 0;

  // two inputs: input and new_sizes
  interpreter->AddTensors(2, &base_index);
  // one output
  interpreter->AddTensors(1, &base_index);
  // set input and output tensors
  interpreter->SetInputs({0, 1});
  interpreter->SetOutputs({2});

  // set parameters of tensors
  TfLiteQuantizationParams quant;
  interpreter->SetTensorParametersReadWrite(
      0, kTfLiteFloat32, "input",
      {1, image_height, image_width, image_channels}, quant);
  interpreter->SetTensorParametersReadWrite(1, kTfLiteInt32, "new_size", {2},
                                            quant);
  interpreter->SetTensorParametersReadWrite(
      2, kTfLiteFloat32, "output",
      {1, wanted_height, wanted_width, wanted_channels}, quant);

  ops::builtin::BuiltinOpResolver resolver;
  const TfLiteRegistration *resize_op =
      resolver.FindOp(BuiltinOperator_RESIZE_BILINEAR, 1);
  auto *params = reinterpret_cast<TfLiteResizeBilinearParams *>(
      malloc(sizeof(TfLiteResizeBilinearParams)));
  params->align_corners = false;
  interpreter->AddNodeWithParameters({0, 1}, {2}, nullptr, 0, params, resize_op,
                                     nullptr);

  interpreter->AllocateTensors();

  // fill input image
  // in[] are integers, cannot do memcpy() directly
  auto input = interpreter->typed_tensor<float>(0);
  for (int i = 0; i < number_of_pixels; i++)
  {
    input[i] = in.data[i];
  }

  // fill new_sizes
  interpreter->typed_tensor<int>(1)[0] = wanted_height;
  interpreter->typed_tensor<int>(1)[1] = wanted_width;

  interpreter->Invoke();

  auto output = interpreter->typed_tensor<float>(2);
  auto output_number_of_pixels =
      wanted_height * wanted_height * wanted_channels;

  for (int i = 0; i < output_number_of_pixels; i++)
  {
    if (is_float)
      out[i] = (output[i] - 128) / 128;
    else
      out[i] = (uint8_t)output[i];
  }
}

}  // namespace mfn
}  // namespace tflite

#endif
