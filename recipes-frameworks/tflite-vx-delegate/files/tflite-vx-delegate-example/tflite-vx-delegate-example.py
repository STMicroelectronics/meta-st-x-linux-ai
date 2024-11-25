# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.

# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

# This application is a simple example of TFLite VX-delegate usage
# The application take as argument a .tflite model and run a simple inference with
# inputs data fill with a default input tensor.

import argparse
import numpy as np
import tflite_runtime.interpreter as tf
from timeit import default_timer as timer
import os

def run_inference(model_path):
    # Load the TFLite model and allocate tensors
    interpreter = tf.Interpreter(model_path=model_path, num_threads = os.cpu_count(), experimental_delegates=[tf.load_delegate('/usr/lib/libvx_delegate.so.2')])
    interpreter.allocate_tensors()

    # Get input and output tensors
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    # Prepare input data
    input_shape = input_details[0]['shape']
    input_dtype = input_details[0]['dtype']
    input_data = np.random.random_sample(input_shape).astype(input_dtype)

    # Set the tensor to point to the input data to be inferred
    interpreter.set_tensor(input_details[0]['index'], input_data)

    # Run the inference
    interpreter.invoke()

    # Get the output data
    output_data = interpreter.get_tensor(output_details[0]['index'])
    return output_data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run inference on a TFLite model using TensorFlow Lite runtime and libvx_delegate.so')
    parser.add_argument('model_path', type=str, help='Path to the TFLite model file')
    args = parser.parse_args()

    output = run_inference(args.model_path)
    print("Inference done !")
    # print("Inference results :", output_data)