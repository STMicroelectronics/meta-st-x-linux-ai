#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import cv2
import numpy as np

class TransformPipeline:
    """Applies a sequence of transformations to an image.

    Args:
        transformations (List[Transform]): List of transformations to apply.

    Example:
        >>> pipeline = TransformPipeline([
        >>>     CenterCropTransform(10),
        >>>     ToTensorTransform(),
        >>> ])
    """
    def __init__(self, transformations):
        self.transformations = transformations
    def __call__(self, image, bounding_boxes=None, labels=None):
        for transform in self.transformations:
            image, bounding_boxes, labels = transform(image, bounding_boxes, labels)
        return image, bounding_boxes, labels


class ConvertToFloat32:
    def __call__(self, image, bounding_boxes=None, labels=None):
        return image.astype(np.float32), bounding_boxes, labels

class SubtractMeanValues:
    def __init__(self, mean_values):
        self.mean_values = np.array(mean_values, dtype=np.float32)
    def __call__(self, image, bounding_boxes=None, labels=None):
        image = image.astype(np.float32)
        image -= self.mean_values
        return image.astype(np.float32), bounding_boxes, labels

class NormalizeBoundingBoxes:
    def __call__(self, image, bounding_boxes=None, labels=None):
        height, width, _ = image.shape
        if bounding_boxes.size != 0:
            bounding_boxes[:, 0] /= width
            bounding_boxes[:, 2] /= width
            bounding_boxes[:, 1] /= height
            bounding_boxes[:, 3] /= height
        return image, bounding_boxes, labels

class ResizeImage:
    def __init__(self, target_width, target_height):
        self.target_width = target_width
        self.target_height = target_height
    def __call__(self, image, bounding_boxes=None, labels=None):
        image = cv2.resize(image, (self.target_width, self.target_height))
        return image, bounding_boxes, labels

class ConvertToFloat32AndTranspose:
    def __call__(self, image, bounding_boxes=None, labels=None):
        # Convert the image to a numpy array of type float32
        image = image.astype(np.float32)
        # Permute the axes to have the channel dimension first (C, H, W)
        image = np.transpose(image, (2, 0, 1))
        return image, bounding_boxes, labels