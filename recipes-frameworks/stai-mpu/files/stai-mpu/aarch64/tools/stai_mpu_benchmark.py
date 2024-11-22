#!/usr/bin/python3
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import argparse
import time
import numpy as np
import psutil
import os

# Assuming stai_mpu_network is a Python class that provides similar functionality to the C++ version
from stai_mpu import stai_mpu_network

# Constants
BILLION = 1000000000

def get_perf_count():
    return time.time() * BILLION

def get_peak_working_set_size():
    process = psutil.Process(os.getpid())
    return process.memory_info().rss

def main():
    parser = argparse.ArgumentParser(description='STAI_MPU Benchmarking Tool')
    parser.add_argument('-m', '--model_file', type=str, required=True, help='.nb network binary file to be benchmarked.')
    parser.add_argument('-l', '--nb_loops', type=int, default=1, help='The number of loops of the inference (default loops=1)')
    args = parser.parse_args()

    model_path = args.model_file
    nb_loops = args.nb_loops
    model_extension = os.path.splitext(model_path)

    # Load the model and metadata
    if model_extension[1] == ".nb":
        stai_mpu_model = stai_mpu_network(model_path, True)
    else :
        stai_mpu_model = stai_mpu_network(model_path, False)
    num_inputs = stai_mpu_model.get_num_inputs()
    num_outputs = stai_mpu_model.get_num_outputs()

    input_infos = stai_mpu_model.get_input_infos()
    output_infos = stai_mpu_model.get_output_infos()

    # Pre-processing the input
    input_shape = input_infos[0].get_shape()
    input_width, input_height, input_channels = input_shape[1], input_shape[2], input_shape[3]
    size_in_bytes = input_height * input_width * input_channels

    # Create dummy input data
    input_data = np.zeros(size_in_bytes, dtype=np.uint8)

    # Setting input and infer
    floating_model = input_infos[0].get_dtype() == 'float32'
    input_tensor = input_data.astype(np.float32) if floating_model else input_data
    stai_mpu_model.set_input(0, input_tensor)

    # Running inference
    print(f"[STAI_MPU][BENCHMARK] Started running the graph [{nb_loops}] loops ...")
    time_start = get_perf_count()
    for _ in range(nb_loops):
        stai_mpu_model.run()
    time_end = get_perf_count()

    peak_memory = get_peak_working_set_size()
    print(f"[STAI_MPU][BENCHMARK] Peak working set size: {peak_memory} bytes")

    inference_time_ms = (time_end - time_start) / 1000000 / nb_loops
    inference_time_us = (time_end - time_start) / 1000 / nb_loops
    print(f"[STAI_MPU][BENCHMARK] Loop:{nb_loops},Average: {inference_time_ms:.2f} ms or {inference_time_us:.2f} us")

if __name__ == "__main__":
    main()