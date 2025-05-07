#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import numpy as np
from collections import Counter
import cv2
from pathlib import Path

path = Path(__file__).parent / "../../student_model/ssd_mobilenet_v2/labels.txt"
with path.open() as f:
    class_names = [name.strip() for name in f.readlines()]

def draw_detections(image, boxes, scores, class_ids, mask_alpha=0.3, bb_images: bool=False):
    det_img = image.copy()
    img_height, img_width = image.shape[:2]
    font_size = min([img_height, img_width]) * 0.0006
    text_thickness = int(min([img_height, img_width]) * 0.001)
    det_img = draw_masks(det_img, boxes, class_ids, mask_alpha)
    # Draw bounding boxes and labels of detections
    for class_id, box, score in zip(class_ids, boxes, scores):
        draw_box(det_img, box, (0, 255, 0))
        label = class_names[0]
        caption = f'{label} {int(score * 100)}%'
        draw_text(det_img, caption, box, (0, 255, 0), font_size, text_thickness)
    return det_img

def draw_box(image: np.ndarray, box: np.ndarray, color: tuple[int, int, int] = (0, 255, 0),
             thickness: int = 2) -> np.ndarray:
    x1, y1, x2, y2 = box.astype(int)
    return cv2.rectangle(image, (x1, y1), (x2, y2), color, thickness)

def draw_text(image: np.ndarray, text: str, box: np.ndarray, color: tuple[int, int, int] = (0, 0, 255),
              font_size: float = 0.001, text_thickness: int = 2) -> np.ndarray:
    x1, y1, x2, y2 = box.astype(int)
    (tw, th), _ = cv2.getTextSize(text=text, fontFace=cv2.FONT_HERSHEY_SIMPLEX,
                                  fontScale=font_size, thickness=text_thickness)
    th = int(th * 1.2)
    cv2.rectangle(image, (x1, y1), (x1 + tw, y1 - th), color, -1)
    return cv2.putText(image, text, (x1, y1), cv2.FONT_HERSHEY_SIMPLEX, font_size, (255, 255, 255), text_thickness, cv2.LINE_AA)

def draw_masks(image: np.ndarray, boxes: np.ndarray, classes: np.ndarray, mask_alpha: float = 0.3) -> np.ndarray:
    mask_img = image.copy()
    # Draw bounding boxes and labels of detections
    for box, class_id in zip(boxes, classes):
        x1, y1, x2, y2 = box.astype(int)
        # Draw fill rectangle in mask image
        cv2.rectangle(mask_img, (x1, y1), (x2, y2), (0, 255, 0), -1)
    return cv2.addWeighted(mask_img, mask_alpha, image, 1 - mask_alpha, 0)
