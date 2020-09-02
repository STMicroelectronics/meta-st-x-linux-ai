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
* Change pre-trained model to MobileFaceNet and add functions of adding new person 
* to database, caculating confidence coefficient, plotting and so on. 
*
* References:
* https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/examples/label_image/label_image.cc
*/

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <experimental/filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <unordered_map>

#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "tensorflow/lite/string_util.h"

#include "ThreadPool.h"

#include "face_detect_helpers.h"
#include "mfn.h"

#include "profiling.h"

#define LOG(x) std::cerr

using namespace cv;

namespace tflite
{
namespace facerec
{

std::unique_ptr<tflite::Interpreter> interpreter;
int wanted_height, wanted_width, wanted_channels;
int input;
const int num_class = 128;
std::unordered_map<std::string, std::pair<Mat, std::array<float, num_class>>> face_database;
std::array<float, num_class> pred;
String window_name("Display window");

void plot(Mat &display)
{
  imshow(window_name, display);
  waitKey(1);
}

std::pair<std::string, float> predict_label(std::array<float, num_class> &pred, float threshold)
{
  float ret = 0, mod1 = 0, mod2 = 0;
  float max = 0;
  std::string label;
  for (int i = 0; i < num_class; i++)
    mod1 += pred[i] * pred[i];
  for (auto &face : face_database)
  {
    ret = 0, mod2 = 0;
    std::array<float, num_class> &vec = face.second.second;
    for (int i = 0; i < num_class; i++)
    {
      ret += pred[i] * vec[i];
      mod2 += vec[i] * vec[i];
    }
    float cosine_similarity = (ret / sqrt(mod1) / sqrt(mod2) + 1) / 2.0;
    if (max <= cosine_similarity)
    {
      max = cosine_similarity;
      label = face.first;
    }
  }
  if (max < threshold)
  {
    label.clear();
  }
  return {label, max};
}

void init_interpreter(Settings *s)
{
  if (!s->model_name.c_str())
  {
    LOG(ERROR) << "no model file name\n";
    exit(-1);
  }

  std::unique_ptr<tflite::FlatBufferModel> model;
  model = tflite::FlatBufferModel::BuildFromBuffer(mfn_tflite, mfn_tflite_len);
  //model = tflite::FlatBufferModel::BuildFromFile("mfn.tflite");

  if (!model)
  {
    LOG(FATAL) << "\nFailed to load model " << s->model_name << "\n";
    exit(-1);
  }
  if (s->verbose)
    LOG(INFO) << "Loaded model " << s->model_name << "\n\r";
  model->error_reporter();
  if (s->verbose)
    LOG(INFO) << "resolved reporter\n\r";

  tflite::ops::builtin::BuiltinOpResolver resolver;

  tflite::InterpreterBuilder(*model, resolver)(&interpreter);
  if (!interpreter)
  {
    LOG(FATAL) << "Failed to construct interpreter\n";
    exit(-1);
  }

  interpreter->UseNNAPI(s->accel);

  if (s->verbose)
  {
    LOG(INFO) << "tensors size: " << interpreter->tensors_size() << "\n\r";
    LOG(INFO) << "nodes size: " << interpreter->nodes_size() << "\n\r";
    LOG(INFO) << "inputs: " << interpreter->inputs().size() << "\n\r";
    LOG(INFO) << "input(0) name: " << interpreter->GetInputName(0) << "\n\r";
  }

  if (s->number_of_threads != -1)
  {
    interpreter->SetNumThreads(s->number_of_threads);
  }

  input = interpreter->inputs()[0];
  if (s->verbose)
    LOG(INFO) << "input: " << input << "\n\r";

  const std::vector<int> inputs = interpreter->inputs();
  const std::vector<int> outputs = interpreter->outputs();

  if (s->verbose)
  {
    LOG(INFO) << "number of inputs: " << inputs.size() << "\n\r";
    LOG(INFO) << "number of outputs: " << outputs.size() << "\n\r";
  }

  if (interpreter->AllocateTensors() != kTfLiteOk)
  {
    LOG(FATAL) << "Failed to allocate tensors!";
  }

  if (s->verbose)
    PrintInterpreterState(interpreter.get());

  // get input dimension from the input tensor metadata
  // assuming one input only
  TfLiteIntArray *dims = interpreter->tensor(input)->dims;
  wanted_height = dims->data[1];
  wanted_width = dims->data[2];
  wanted_channels = dims->data[3];
}

std::array<float, num_class> get_output_tensor(Mat img, Settings *s)
{
  switch (interpreter->tensor(input)->type)
  {
  case kTfLiteFloat32:
    copy_resize(interpreter->typed_tensor<float>(input), img,
                wanted_height, wanted_width, wanted_channels, true);
    break;
  case kTfLiteUInt8:
    copy_resize(interpreter->typed_tensor<uint8_t>(input), img,
                wanted_height, wanted_width, wanted_channels, false);
    break;
  default:
    LOG(FATAL) << "cannot handle input type "
               << interpreter->tensor(input)->type << " yet";
    exit(-1);
  }

  auto start_time = GET_TIME_IN_US();
  if (interpreter->Invoke() != kTfLiteOk)
  {
    LOG(FATAL) << "Failed to invoke tflite!\n";
  }
  auto stop_time = GET_TIME_IN_US();
  if (s->profiling)
  {
    LOG(INFO) << "tensorflow lite invoked\n\r";
    LOG(INFO) << "total time: "
              << (GET_TIME_DIFF(start_time, stop_time)) / 1000
              << " ms \n\r";
  }

  int output = interpreter->outputs()[0];
  TfLiteTensor *output_tensor = interpreter->tensor(output);
  TfLiteIntArray *output_dims = output_tensor->dims;
  // assume output dims to be something like (1, 1, ... ,size)
  auto output_size = output_dims->data[output_dims->size - 1];
  std::array<float, num_class> array;
  switch (interpreter->tensor(output)->type)
  {
  case kTfLiteFloat32:
  {
    float *output_float = interpreter->typed_output_tensor<float>(0);
    for (int i = 0; i < output_size; i++)
      array[i] = output_float[i];
    break;
  }
  case kTfLiteUInt8:
  {
    uint8_t *output_uint8 = interpreter->typed_output_tensor<uint8_t>(0);
    for (int i = 0; i < output_size; i++)
      array[i] = output_uint8[i];
    break;
  }
  default:
  {
    LOG(FATAL) << "cannot handle output type "
               << interpreter->tensor(input)->type << " yet";
    exit(-1);
  }
  }
  return array;
}

void init_database(Settings *s)
{
  if (s->verbose)
    LOG(INFO) << "Enrolling...." << "\r\n";
  std::vector<String> image_names;
  const int dir_err = mkdir(s->data_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (dir_err != -1 && errno != EEXIST)
  {
    return;
  }
  glob(s->data_dir, image_names);

  auto start_time = GET_TIME_IN_US();
  for (size_t i = 0; i < image_names.size(); i++)
  {
    if (s->verbose)
      LOG(INFO) << "Add face " << i + 1 << "\r\n";
    Mat img = imread(image_names[i], IMREAD_COLOR);
    Mat face = get_data_face(img);
    if (face.empty())
    {
      continue;
    }
    std::array<float, num_class> feature_vec = get_output_tensor(face, s);
    cvtColor(face, face, COLOR_RGB2BGR);
    std::string label = image_names[i];
    label.erase(0, s->data_dir.length());
    label.erase(label.find("."));
    face_database[label] = {face, feature_vec};
  }
  auto stop_time = GET_TIME_IN_US();
  if (s->profiling)
  {
    LOG(INFO) << "init face database\n\r";
    LOG(INFO) << "total time: "
              << (GET_TIME_DIFF(start_time, stop_time)) / 1000
              << " ms \n\r";
    LOG(INFO) << "average time: "
              << (GET_TIME_DIFF(start_time, stop_time)) / 1000 / image_names.size()
              << " ms \n\r";
  }
  if (s->verbose)
    LOG(INFO) << "The all " << image_names.size() << "faces database is done!\r\n";
}

void run_inference(Settings *s)
{
  Mat &display = get_display();
  std::future<Mat> face_future;
  Mat face;
  std::future<std::array<float, num_class>> pred_future;
  std::array<float, num_class> pred;
  std::future<std::pair<std::string, float>> result_future;
  std::pair<std::string, float> result;
  int no_match = 0;
  const int stable_threshold = 5;

  auto start_time = GET_TIME_IN_US();
  auto stop_time = GET_TIME_IN_US();

  ThreadPool pool(3);
  while (true)
  {
    plot(display);
    if (is_pause())
    {
      continue;
    }

    stop_time = GET_TIME_IN_US();
    if (s->profiling)
    {
      LOG(INFO) << "run inference \n\r";
      LOG(INFO) << "total time: "
                << (GET_TIME_DIFF(start_time, stop_time)) / 1000
                << " ms \n\r";
    }
    start_time = GET_TIME_IN_US();

    if (face_future.valid())
      face = face_future.get();

    if (pred_future.valid())
    {
      pred = pred_future.get();
      result_future = pool.enqueue(predict_label, pred, s->threshold);
    }

    face_future = pool.enqueue(get_test_face);
    if (!face.empty())
      pred_future = pool.enqueue(get_output_tensor, face, s);
      
    if (result_future.valid())
    {
      result = result_future.get();
      std::string& label = result.first;
      if (!label.empty())
      {
        no_match = 0;
        show_matched_face(face_database[label].first, label, result.second);
        continue;
      }
    }
    if (no_match >= stable_threshold)
    {
      no_matched_face();
    }
    no_match += 1;
  }
}

void add_new_face(Mat img, std::string label, Settings *s)
{
  while (label.empty())
  {
    std::cout << "Enter image label: ";
    if (!std::getline(std::cin, label))
    {
      LOG(FATAL) << "I/O Eror\n\r";
      return;
    }
  }
  auto start_time = GET_TIME_IN_US();
  String img_name = s->data_dir + label + ".bmp";
  imwrite(img_name, img);

  Mat face = get_data_face(img);
  if (!face.empty())
  {
    std::array<float, num_class> feature_vec = get_output_tensor(face, s);
    cvtColor(face, face, COLOR_RGB2BGR);
    face_database[label] = {face, feature_vec};
  }
  auto stop_time = GET_TIME_IN_US();
  if (s->profiling)
  {
    LOG(INFO) << "add new face to the database\n\r";
    LOG(INFO) << "total time: "
              << (GET_TIME_DIFF(start_time, stop_time)) / 1000
              << " ms \n\r";
  }
}

void display_usage()
{
  LOG(INFO) << "label_image\n"
            << "--accelerated, -a: [0|1], use Android NNAPI or not\n"
            << "--camera, -c: camera index\n"
            << "--data_dir -i: data/images/dir/\n"
            << "--profiling, -p: [0|1], profiling or not\n"
            << "--tflite_model, -m: model_name.tflite\n"
            << "--threads, -t: number of threads\n"
            << "--threshold, -h: threshold for the predict score\n"
            << "--verbose, -v: [0|1] print more information\n"
            << "\n";
}

int Main(int argc, char **argv)
{
  Settings s;

  int c;
  while (1)
  {
    static struct option long_options[] = {
        {"accelerated", required_argument, nullptr, 'a'},
        {"camera", required_argument, nullptr, 'c'},
        {"verbose", required_argument, nullptr, 'v'},
        {"data_dir", required_argument, nullptr, 'i'},
        {"tflite_model", required_argument, nullptr, 'm'},
        {"profiling", required_argument, nullptr, 'p'},
        {"threads", required_argument, nullptr, 't'},
        {"threshold", required_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}};

    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long(argc, argv, "a:c:h:i:m:p:t:v:", long_options,
                    &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
    {
    case 'a':
      s.accel = strtol(optarg, nullptr, 10);
      break;
    case 'c':
      s.camera = strtol(optarg, nullptr, 10);
      break;
    case 'h':
      s.threshold = strtod(optarg, nullptr);
      break;
    case 'i':
      s.data_dir = optarg;
      break;
    case 'm':
      s.model_name = optarg;
      break;
    case 'p':
      s.profiling = strtol(optarg, nullptr, 10);
      break;
    case 't':
      s.number_of_threads = strtol(optarg, nullptr, 10);
      break;
    case 'v':
      s.verbose = strtol(optarg, nullptr, 10);
      break;
    case '?':
      /* getopt_long already printed an error message. */
      display_usage();
      exit(-1);
    default:
      exit(-1);
    }
  }
  init_face_detect(&s);
  init_interpreter(&s);
  init_database(&s);
  init_window();
  run_inference(&s);
  return 0;
}

} // namespace facerec
} // namespace tflite

int main(int argc, char **argv)
{
  return tflite::facerec::Main(argc, argv);
}
