/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

/*
 * This application is a simple example of TFLite VX-delegate usage
 * The application take as argument a .tflite model and run a simple inference with
 * inputs data fill with a default input tensor.
 * The application support common I/O types uint8, int8, float32, to support more types
 * please refer to the TensorFlow Lite documentation
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <thread>
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "tensorflow/lite/delegates/external/external_delegate.h"
#include "tensorflow/lite/interpreter.h"
#include "VX/vsi_npu_custom_op.h"

#define LOG(x) std::cerr

void generate_input_data(const TfLiteTensor* tensor, std::vector<uint8_t>& uint8_data, std::vector<int8_t>& int8_data, std::vector<float>& float_data) {
    switch (tensor->type) {
        case kTfLiteUInt8: {
            uint8_data.resize(tensor->bytes);
            std::fill(uint8_data.begin(), uint8_data.end(), 128);  // Example: fill with 128
            std::cout << "Generated uint8 input data." << std::endl;
            break;
        }
        case kTfLiteInt8: {
            int8_data.resize(tensor->bytes);
            std::fill(int8_data.begin(), int8_data.end(), 0);  // Example: fill with 0
            std::cout << "Generated int8 input data." << std::endl;
            break;
        }
        case kTfLiteFloat32: {
            float_data.resize(tensor->bytes / sizeof(float));
            std::fill(float_data.begin(), float_data.end(), 0.5f);  // Example: fill with 0.5
            std::cout << "Generated float32 input data." << std::endl;
            break;
        }
        default:
            std::cout << "This simple example application only support common I/O types : int8, uint8, float32 " << std::endl;
            std::cout << "Other types can be support please refer to TensorFlow Lite documentation " << std::endl;
            std::cerr << "Unsupported input tensor type: " << TfLiteTypeGetName(tensor->type) << std::endl;
            break;
    }
}

void retrieve_output_data(const TfLiteTensor* tensor) {
    switch (tensor->type) {
        case kTfLiteUInt8: {
            const uint8_t* output_data = tensor->data.uint8;
            std::cout << "Output data (uint8): ";
            for (int i = 0; i < tensor->bytes; ++i) {
                std::cout << static_cast<int>(output_data[i]) << " ";
            }
            std::cout << std::endl;
            break;
        }
        case kTfLiteInt8: {
            const int8_t* output_data = tensor->data.int8;
            std::cout << "Output data (int8): ";
            for (int i = 0; i < tensor->bytes; ++i) {
                std::cout << static_cast<int>(output_data[i]) << " ";
            }
            std::cout << std::endl;
            break;
        }
        case kTfLiteFloat32: {
            const float* output_data = tensor->data.f;
            std::cout << "Output data (float32): ";
            for (int i = 0; i < tensor->bytes / sizeof(float); ++i) {
                std::cout << output_data[i] << " ";
            }
            std::cout << std::endl;
            break;
        }
        default:
            std::cout << "This simple example application only support common I/O types : int8, uint8, float32 " << std::endl;
            std::cout << "Other types can be support please refer to TensorFlow Lite documentation " << std::endl;
            std::cerr << "Unsupported output tensor type: " << TfLiteTypeGetName(tensor->type) << std::endl;
            break;
    }
}

void run_inference(const std::string& model_file) {
    // Load the TFLite model
    std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> interpreter;
    model = tflite::FlatBufferModel::BuildFromFile(model_file.c_str());
    if (!model) {
        LOG(FATAL) << "\nFailed to mmap model " << model_file << "\n";
        exit(-1);
    }
    LOG(INFO) << "Loaded model " << model_file << "\n";
    model->error_reporter();


    /* Get number of CPU cores available */
    uint8_t nb_cpu_cores = 1;
    const auto processor_count = std::thread::hardware_concurrency();
    std::cout << processor_count << " cpu core(s) available\n";
    if (processor_count)
        nb_cpu_cores = (uint8_t)processor_count;

    // Build the interpreter
    tflite::ops::builtin::BuiltinOpResolver resolver;
    resolver.AddCustom(kNbgCustomOp, tflite::ops::custom::Register_VSI_NPU_PRECOMPILED());
    tflite::InterpreterBuilder(*model, resolver)(&interpreter);
    if (!interpreter) {
        LOG(FATAL) << "Failed to construct interpreter\n";
        exit(-1);
    }

    const char * delegate_path = "/usr/lib/libvx_delegate.so.2";
    auto ext_delegate_option = TfLiteExternalDelegateOptionsDefault(delegate_path);
    // ext_delegate_option.insert(&ext_delegate_option, "cache_file_path", "path/to/nbfile");
    // ext_delegate_option.insert(&ext_delegate_option, "allowed_cache_mode", "true");
    auto ext_delegate_ptr = TfLiteExternalDelegateCreate(&ext_delegate_option);
    interpreter->ModifyGraphWithDelegate(ext_delegate_ptr);

    if (nb_cpu_cores != -1) {
        interpreter->SetNumThreads(nb_cpu_cores);
    }

    // Allocate tensor buffers
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        LOG(FATAL) << "Failed to allocate tensors!";
    }

    // Get input tensor
    const TfLiteTensor* input_tensor = interpreter->input_tensor(0);

    // Generate input data based on the input tensor's data type
    std::vector<uint8_t> uint8_data;
    std::vector<int8_t> int8_data;
    std::vector<float> float_data;
    generate_input_data(input_tensor, uint8_data, int8_data, float_data);

    // Set input data to the interpreter
    switch (input_tensor->type) {
        case kTfLiteUInt8:
            std::memcpy(interpreter->typed_input_tensor<uint8_t>(0), uint8_data.data(), uint8_data.size());
            break;
        case kTfLiteInt8:
            std::memcpy(interpreter->typed_input_tensor<int8_t>(0), int8_data.data(), int8_data.size());
            break;
        case kTfLiteFloat32:
            std::memcpy(interpreter->typed_input_tensor<float>(0), float_data.data(), float_data.size() * sizeof(float));
            break;
        default:
            std::cout << "This simple example application only support common I/O types : int8, uint8, float32 " << std::endl;
            std::cout << "Other types can be support please refer to TensorFlow Lite documentation " << std::endl;
            std::cerr << "Unsupported input tensor type: " << TfLiteTypeGetName(input_tensor->type) << std::endl;
            return;
    }

    // Run inference
    if (interpreter->Invoke() != kTfLiteOk) {
        LOG(FATAL) << "Failed to invoke tflite!\n";
    } else {
        LOG(INFO) << "Inference done ! \n";
    }

    // Retrieve and print output data for each output tensor
    for (int i = 0; i < interpreter->outputs().size(); ++i) {
        int output_index = interpreter->outputs()[i];
        const TfLiteTensor* output_tensor = interpreter->output_tensor(output_index);
        // Printing outputs  is disable by default
        // std::cout << "Output Tensor " << i << ":" << std::endl;
        // retrieve_output_data(output_tensor);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <model_path>\n", argv[0]);
        return 1;
    }

    std::string model_path = argv[1];
    run_inference(model_path);
    return 0;
}