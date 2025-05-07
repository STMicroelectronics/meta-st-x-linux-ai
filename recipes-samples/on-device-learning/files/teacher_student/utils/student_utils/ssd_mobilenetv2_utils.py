#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import numpy as np
from utils.student_utils.postprocessing_utils import SSDAnchorsParams, SSDBoxSizes

image_mean = np.array([127, 127, 127])  # RGB layout
image_std = 128.0
iou_threshold = 0.45
center_variance = 0.1
size_variance = 0.2

specs = [
    SSDAnchorsParams(19, 16, SSDBoxSizes(60, 105),  [2, 3]),
    SSDAnchorsParams(10, 32, SSDBoxSizes(105, 150), [2, 3]),
    SSDAnchorsParams(5, 64,  SSDBoxSizes(150, 195), [2, 3]),
    SSDAnchorsParams(3, 100, SSDBoxSizes(195, 240), [2, 3]),
    SSDAnchorsParams(2, 150, SSDBoxSizes(240, 285), [2, 3]),
    SSDAnchorsParams(1, 300, SSDBoxSizes(285, 330), [2, 3])
]
