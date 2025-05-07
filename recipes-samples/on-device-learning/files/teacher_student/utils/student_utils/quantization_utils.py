#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import numpy as np
import cv2
import os
from onnxruntime import InferenceSession
from onnxruntime.quantization import CalibrationDataReader

from utils.dataset_utils.preprocessing_utils import InferenceTransform


def _preprocess_images(image_directory: str, target_height: int, target_width: int, nb_images=10):
    """
    Loads a batch of images and preprocess them
    parameter image_directory: path to folder storing images
    parameter target_height: image target_height in pixels
    parameter target_width: image target_width in pixels
    parameter nb_images: number of images to load.
    return: list of matrices characterizing multiple images
    """
    preprocessed_images = []
    image_count = 0
    for img_filename in sorted(os.listdir(image_directory)):
        if image_count > nb_images:
            break
        image = cv2.imread(image_directory + f"/{img_filename}")
        if image is None:
            continue
        image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        image = InferenceTransform(target_width, target_height, 127, 128.0)(image)
        input_tensor = np.expand_dims(image, axis=0)
        preprocessed_images.append(input_tensor)
        image_count += 1
    batch_images = np.concatenate(np.expand_dims(preprocessed_images, axis=0), axis=0)

    return batch_images


class SSDDataReader(CalibrationDataReader):
    def __init__(self, calibration_image_folder: str, model_path: str, nb_images: int):
        self.enum_data = None
        # Use inference session to get input shape.
        session = InferenceSession(model_path, None, providers=['CPUExecutionProvider'])
        (_, _, target_height, target_width) = session.get_inputs()[0].shape
        # Convert image to input data
        self.nhwc_data_list = _preprocess_images(
            calibration_image_folder, target_height, target_width, nb_images)
        self.input_name = session.get_inputs()[0].name

    def get_next(self):
        if self.enum_data is None:
            self.enum_data = iter(
                [{self.input_name: nhwc_data}
                    for nhwc_data in self.nhwc_data_list]
            )
        return next(self.enum_data, None)

    def rewind(self):
        self.enum_data = None
