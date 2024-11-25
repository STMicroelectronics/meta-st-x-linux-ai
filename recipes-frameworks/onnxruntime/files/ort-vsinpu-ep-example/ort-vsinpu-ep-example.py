# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.

# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

# This application is a simple example of ONNX VSINPU Execution provider usage
# The application take as argument a .onnx model and run a simple inference with
# inputs data fill with a default input tensor.

import argparse
import numpy as np
import onnxruntime as ort
from timeit import default_timer as timer
import os

def run_inference(model_path):
    # Load the ONNX model and create an inference session with VSI NPU execution provider
    session_options = ort.SessionOptions()
    session_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    session = ort.InferenceSession(model_path, sess_options=session_options, providers=['VSINPUExecutionProvider'])

    # Get input and output details
    input_name = session.get_inputs()[0].name
    input_shape = session.get_inputs()[0].shape
    input_type = session.get_inputs()[0].type

    # Map ONNX type to numpy type
    type_mapping = {
        'tensor(float)': np.float32,
        'tensor(float16)': np.float16,
        'tensor(double)': np.float64,
        'tensor(int32)': np.int32,
        'tensor(int64)': np.int64,
        'tensor(int8)': np.int8,
        'tensor(uint8)': np.uint8,
        'tensor(bool)': np.bool_
    }

    input_dtype = type_mapping.get(input_type, np.float32)  # Default to float32 if type is not found

    # Prepare input data
    input_data = np.random.random_sample(input_shape).astype(input_dtype)

    # Run the inference
    output_data = session.run(None, {input_name: input_data})

    return output_data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run inference on an ONNX model using ONNX Runtime and VSI NPU execution provider')
    parser.add_argument('model_path', type=str, help='Path to the ONNX model file')
    args = parser.parse_args()

    output = run_inference(args.model_path)
    print("Inference done!")
    # print("Inference results:", output)