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
 * This application is a simple example of ONNX Runtime VSINPU usage
 * The application take as argument a .onnx model and run a simple inference with
 * inputs data fill with a default input tensor.
 * The application support common I/O types uint8, int8, float32, to support more types
 * please refer to the ONNX Runtime documentation
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <thread>
#include "onnxruntime_cxx_api.h"
#include "core/providers/vsinpu/vsinpu_provider_factory.h"
#include "onnxruntime_session_options_config_keys.h"

#define LOG(x) std::cerr

void generate_input_data(ONNXTensorElementDataType type, std::vector<uint8_t>& uint8_data, std::vector<int8_t>& int8_data, std::vector<float>& float_data, size_t size) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
            uint8_data.resize(size);
            std::fill(uint8_data.begin(), uint8_data.end(), 128);  // Example: fill with 128
            std::cout << "Generated uint8 input data." << std::endl;
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
            int8_data.resize(size);
            std::fill(int8_data.begin(), int8_data.end(), 0);  // Example: fill with 0
            std::cout << "Generated int8 input data." << std::endl;
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: {
            float_data.resize(size);
            std::fill(float_data.begin(), float_data.end(), 0.5f);  // Example: fill with 0.5
            std::cout << "Generated float32 input data." << std::endl;
            break;
        }
        default:
            std::cout << "This simple example application only support common I/O types : int8, uint8, float32 " << std::endl;
            std::cout << "Other types can be support please refer to ONNX Runtime documentation " << std::endl;
            std::cerr << "Unsupported output tensor type: " << type << std::endl;
            break;
    }
}

void retrieve_output_data(ONNXTensorElementDataType type, const void* data, size_t size) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
            const uint8_t* output_data = static_cast<const uint8_t*>(data);
            std::cout << "Output data (uint8): ";
            for (size_t i = 0; i < size; ++i) {
                std::cout << static_cast<int>(output_data[i]) << " ";
            }
            std::cout << std::endl;
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
            const int8_t* output_data = static_cast<const int8_t*>(data);
            std::cout << "Output data (int8): ";
            for (size_t i = 0; i < size; ++i) {
                std::cout << static_cast<int>(output_data[i]) << " ";
            }
            std::cout << std::endl;
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: {
            const float* output_data = static_cast<const float*>(data);
            std::cout << "Output data (float32): ";
            for (size_t i = 0; i < size; ++i) {
                std::cout << output_data[i] << " ";
            }
            std::cout << std::endl;
            break;
        }
        default:
            std::cout << "This simple example application only support common I/O types : int8, uint8, float32 " << std::endl;
            std::cout << "Other types can be support please refer to ONNX Runtime documentation " << std::endl;
            std::cerr << "Unsupported output tensor type: " << type << std::endl;
            break;
    }
}

void run_inference(const std::string& model_file) {

    /* create an environment and session options */
    Ort::Env ort_env(ORT_LOGGING_LEVEL_WARNING, "Onnx_environment");
    Ort::SessionOptions session_options;

    // Set VSINPU AI execution provider
    OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_VSINPU(session_options);
    if (status != nullptr) {
        std::cerr << "Failed to set VSINPU AI execution provider: " << Ort::GetApi().GetErrorMessage(status) << std::endl;
        Ort::GetApi().ReleaseStatus(status);
        throw std::runtime_error("[ORT] Failed: VSINPU AI execution provider runtime error");
    }

     /* Get number of CPU cores available */
	int nb_cpu_cores = 1;
	const auto processor_count = std::thread::hardware_concurrency();
	std::cout << processor_count << " cpu core(s) available\n";
	if (processor_count)
		nb_cpu_cores = (uint8_t)processor_count;

    session_options.DisableCpuMemArena();
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    /* create a session from the ONNX model file */
    Ort::Session session(ort_env, model_file.c_str(), session_options);
    if (session == nullptr) {
        ORT_CXX_API_THROW("", OrtErrorCode::ORT_NO_MODEL );
    }

    Ort::MemoryInfo memory_info_ = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator,
                                                        OrtMemType::OrtMemTypeDefault);

    Ort::AllocatorWithDefaultOptions allocator;
    // Get input node information
    size_t num_input_nodes = session.GetInputCount();
    std::vector<Ort::AllocatedStringPtr> input_names_ptrs;
    std::vector<char*>  input_node_names(num_input_nodes);
    std::vector<ONNXTensorElementDataType> input_types(num_input_nodes);
    std::vector<std::vector<int64_t>> input_shapes(num_input_nodes);

    for (size_t i = 0; i < num_input_nodes; ++i) {
        input_names_ptrs.push_back(session.GetInputNameAllocated(i, allocator));
        input_node_names[i] = input_names_ptrs.back().get();
        Ort::TypeInfo type_info = session.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        input_types[i] = tensor_info.GetElementType();
        input_shapes[i] = tensor_info.GetShape();
    }

    // Generate input data
    std::vector<uint8_t> uint8_data;
    std::vector<int8_t> int8_data;
    std::vector<float> float_data;
    for (size_t i = 0; i < num_input_nodes; ++i) {
        size_t size = 1;
        for (auto dim : input_shapes[i]) {
            size *= dim;
        }
        generate_input_data(input_types[i], uint8_data, int8_data, float_data, size);
    }

    // Create input tensor
    std::vector<Ort::Value> input_tensors;
    for (size_t i = 0; i < num_input_nodes; ++i) {
        size_t size = 1;
        for (auto dim : input_shapes[i]) {
            size *= dim;
        }
        switch (input_types[i]) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
                input_tensors.push_back(Ort::Value::CreateTensor<uint8_t>(memory_info_, uint8_data.data(), size, input_shapes[i].data(), input_shapes[i].size()));
                break;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
                input_tensors.push_back(Ort::Value::CreateTensor<int8_t>(memory_info_, int8_data.data(), size, input_shapes[i].data(), input_shapes[i].size()));
                break;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
                input_tensors.push_back(Ort::Value::CreateTensor<float>(memory_info_, float_data.data(), size, input_shapes[i].data(), input_shapes[i].size()));
                break;
            default:
                std::cout << "This simple example application only support common I/O types : int8, uint8, float32 " << std::endl;
                std::cout << "Other types can be support please refer to ONNX Runtime documentation " << std::endl;
                std::cerr << "Unsupported output tensor type: " << input_types[i] << std::endl;
                return;
        }
    }

    // Get output node information
    size_t num_output_nodes = session.GetOutputCount();
    std::vector<Ort::AllocatedStringPtr> output_names_ptrs;
    std::vector<const char*> output_node_names(num_output_nodes);
    std::vector<ONNXTensorElementDataType> output_types(num_output_nodes);
    std::vector<std::vector<int64_t>> output_shapes(num_output_nodes);

    for (size_t i = 0; i < num_output_nodes; ++i) {
        output_names_ptrs.push_back(session.GetOutputNameAllocated(i, allocator));
        output_node_names[i] = output_names_ptrs.back().get();
        Ort::TypeInfo type_info = session.GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        output_types[i] = tensor_info.GetElementType();
        output_shapes[i] = tensor_info.GetShape();
    }

    // Run inference
    std::vector<Ort::Value> output_tensors = session.Run(Ort::RunOptions{nullptr}, input_node_names.data(), input_tensors.data(), num_input_nodes, output_node_names.data(), num_output_nodes);

    std::cout << "Inference run successfully." << std::endl;

    // Retrieve and print output data for each output tensor
    for (size_t i = 0; i < num_output_nodes; ++i) {
        size_t size = 1;
        for (auto dim : output_shapes[i]) {
            size *= dim;
        }
        // std::cout << "Output Tensor " << i << ":" << std::endl;
        // retrieve_output_data(output_types[i], output_tensors[i].GetTensorData<void>(), size);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <model_file>\n", argv[0]);
        return 1;
    }

    std::string model_file = argv[1];
    run_inference(model_file);
    return 0;
}