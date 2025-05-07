#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

from utils.dataset_utils.transforms_utils import *

class TrainSetTransform:
    def __init__(self, input_size, input_mean=0, std=1.0):
        self.input_mean = input_mean
        self.input_size = input_size
        self.transformation = TransformPipeline([
            ConvertToFloat32(),
            NormalizeBoundingBoxes(),
            ResizeImage(self.input_size, self.input_size),
            SubtractMeanValues(self.input_mean),
            lambda img, boxes=None, labels=None: (img / std, boxes, labels),
            ConvertToFloat32AndTranspose(),
        ])

    def __call__(self, img, boxes, labels):
        return self.transformation(img, boxes, labels)

class TestSetTransform:
    def __init__(self, input_size, mean=0.0, std=1.0):
        self.transformation = TransformPipeline([
            NormalizeBoundingBoxes(),
            ResizeImage(input_size, input_size),
            SubtractMeanValues(mean),
            lambda img, boxes=None, labels=None: (img / std, boxes, labels),
            ConvertToFloat32AndTranspose(),
        ])
    def __call__(self, image, boxes, labels):
        return self.transformation(image, boxes, labels)

class InferenceTransform:
    def __init__(self, input_width, input_height, mean=0.0, std=1.0):
        self.transformation = TransformPipeline([
            ResizeImage(input_width, input_height),
            SubtractMeanValues(mean),
            lambda img, boxes=None, labels=None: (img / std, boxes, labels),
            ConvertToFloat32AndTranspose(),
        ])
    def __call__(self, image):
        image, _, _ = self.transformation(image)
        return image
