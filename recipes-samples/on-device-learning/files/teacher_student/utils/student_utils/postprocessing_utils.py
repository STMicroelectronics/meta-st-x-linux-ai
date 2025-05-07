
#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import collections
from typing import List
import itertools
import math
import numpy as np

SSDBoxSizes = collections.namedtuple('SSDBoxSizes', ['min', 'max'])

SSDAnchorsParams = collections.namedtuple(
    'SSDAnchorsParams', ['feature_map_size', 'shrinkage', 'box_sizes', 'aspect_ratios'])


def generate_ssd_anchors(params: List[SSDAnchorsParams], image_size, clamp=True):
    """Generate SSD Prior Boxes.

    It returns the center, height and width of the anchors. The values are relative to the image size
    Args:
        params: SSDAnchorsParams about the shapes of sizes of prior boxes. i.e.
        image_size: image size.
        clamp: if true, clamp the values to make fall between [0.0, 1.0]
    Returns:
        anchors (num_anchors, 4): The prior boxes represented as [[center_x, center_y, w, h]]. All the values
            are relative to the image size.
    """
    anchors = []
    for param in params:
        scale = image_size / param.shrinkage
        for j, i in itertools.product(range(param.feature_map_size), repeat=2):
            x_center = (i + 0.5) / scale
            y_center = (j + 0.5) / scale

            # small sized square box
            size = param.box_sizes.min
            h = w = size / image_size
            anchors.append([x_center, y_center, w, h])

            # big sized square box
            size = math.sqrt(param.box_sizes.max * param.box_sizes.min)
            h = w = size / image_size
            anchors.append([x_center, y_center, w, h])

            # change h/w ratio of the small sized box
            size = param.box_sizes.min
            h = w = size / image_size
            for ratio in param.aspect_ratios:
                ratio = math.sqrt(ratio)
                anchors.append([x_center, y_center, w * ratio, h / ratio])
                anchors.append([x_center, y_center, w / ratio, h * ratio])

    anchors = np.array(anchors, dtype=np.float32)
    if clamp:
        np.clip(anchors, 0.0, 1.0, out=anchors)
    return anchors


def convert_locations_to_boxes(locations, anchors, center_variance,
                               size_variance):
    """Convert regressional location results of SSD into boxes in the form of (center_x, center_y, h, w)"""
    # anchors can have one dimension less.
    if len(anchors.shape) + 1 == len(locations.shape):
        anchors = np.expand_dims(anchors, 0)
    return np.concatenate([
        locations[..., :2] * center_variance *
        anchors[..., 2:] + anchors[..., :2],
        np.exp(locations[..., 2:] * size_variance) * anchors[..., 2:]
    ], axis=len(locations.shape) - 1)


def convert_boxes_to_locations(center_form_boxes, center_form_anchors, center_variance, size_variance, epsilon=1e-6):
    # anchors can have one dimension less
    if len(center_form_anchors.shape) + 1 == len(center_form_boxes.shape):
        center_form_anchors = np.expand_dims(center_form_anchors, 0)
    # Ensure no division by zero
    center_form_anchors[..., 2:] = np.maximum(
        center_form_anchors[..., 2:], epsilon)
    center_form_boxes[..., 2:] = np.maximum(
        center_form_boxes[..., 2:], epsilon)
    return np.concatenate([
        (center_form_boxes[..., :2] - center_form_anchors[...,
         :2]) / center_form_anchors[..., 2:] / center_variance,
        np.log(center_form_boxes[..., 2:] /
               center_form_anchors[..., 2:]) / size_variance
    ], axis=len(center_form_boxes.shape) - 1)

def area_of(left_top, right_bottom):
    # Compute the areas of rectangles given two corners.
    hw = np.clip(right_bottom - left_top, 0.0, None)
    return hw[..., 0] * hw[..., 1]

def iou_of(boxes0, boxes1, eps=1e-5):
    # Return intersection-over-union (Jaccard index) of boxes.
    overlap_left_top = np.maximum(boxes0[..., :2], boxes1[..., :2])
    overlap_right_bottom = np.minimum(boxes0[..., 2:], boxes1[..., 2:])

    overlap_area = area_of(overlap_left_top, overlap_right_bottom)
    area0 = area_of(boxes0[..., :2], boxes0[..., 2:])
    area1 = area_of(boxes1[..., :2], boxes1[..., 2:])
    return overlap_area / (area0 + area1 - overlap_area + eps)

def assign_anchors(gt_boxes, gt_labels, corner_form_anchors, iou_threshold):
    # Assign ground truth boxes and targets to anchors.
    num_anchors = corner_form_anchors.shape[0]
    if gt_boxes.size == 0:
        # No ground truth boxes, set all anchors to background class
        boxes = np.zeros((num_anchors, 4), dtype=np.float32)
        labels = np.zeros((num_anchors,), dtype=np.int64)
        return boxes, labels
    # Calculate the IOU between gt_boxes and corner_form_anchors
    ious = iou_of(np.expand_dims(gt_boxes, axis=0), np.expand_dims(corner_form_anchors, axis=1))
    # Find the best ground truth for each prior
    best_target_per_prior_index = np.argmax(ious, axis=1)
    best_target_per_prior = ious[np.arange(ious.shape[0]), best_target_per_prior_index]
    # Find the best prior for each ground truth
    best_prior_per_target_index = np.argmax(ious, axis=0)
    best_prior_per_target = ious[best_prior_per_target_index, np.arange(ious.shape[1])]
    # Ensure every ground truth is assigned to at least one prior
    for target_index, prior_index in enumerate(best_prior_per_target_index):
        best_target_per_prior_index[prior_index] = target_index
    # Using 2 to ensure assignment
    best_target_per_prior[best_prior_per_target_index] = 2.0
    # Set labels for each prior
    labels = gt_labels[best_target_per_prior_index]
    labels[best_target_per_prior < iou_threshold] = 0  # Background class
    # Set boxes for each prior
    boxes = gt_boxes[best_target_per_prior_index]
    return boxes, labels

def center_form_to_corner_form(locations):
    return np.concatenate([locations[..., :2] - locations[..., 2:]/2,
                           locations[..., :2] + locations[..., 2:]/2], len(locations.shape) - 1)

def corner_form_to_center_form(boxes):
    return np.concatenate([(boxes[..., :2] + boxes[..., 2:]) / 2,
                            boxes[..., 2:] - boxes[..., :2]], len(boxes.shape) - 1)

def hard_nms(box_scores, iou_threshold, top_k=-1, candidate_size=200):
    """
    Args:
        box_scores (N, 5): boxes in corner-form and probabilities.
        iou_threshold: intersection over union threshold.
        top_k: keep top_k results. If k <= 0, keep all the results.
        candidate_size: only consider the candidates with the highest scores.
    Returns:
         picked: a list of indexes of the kept boxes
    """
    scores = box_scores[:, -1]
    boxes = box_scores[:, :-1]
    picked = []
    indexes = np.argsort(scores)
    indexes = indexes[-candidate_size:]
    while len(indexes) > 0:
        current = indexes[-1]
        picked.append(current)
        if 0 < top_k == len(picked) or len(indexes) == 1:
            break
        current_box = boxes[current, :]
        indexes = indexes[:-1]
        rest_boxes = boxes[indexes, :]
        iou = iou_of(
            rest_boxes,
            np.expand_dims(current_box, axis=0),
        )
        indexes = indexes[iou <= iou_threshold]

    return box_scores[picked, :]


class AnchorsMatcher(object):
    def __init__(self, center_form_anchors, center_variance, size_variance, iou_threshold):
        self.center_form_anchors = center_form_anchors
        self.corner_form_anchors = center_form_to_corner_form(
            center_form_anchors)
        self.center_variance = center_variance
        self.size_variance = size_variance
        self.iou_threshold = iou_threshold

    def __call__(self, gt_boxes, gt_labels):
        boxes, labels = assign_anchors(gt_boxes, gt_labels,
                                      self.corner_form_anchors, self.iou_threshold)
        boxes = corner_form_to_center_form(boxes)
        locations = convert_boxes_to_locations(
            boxes, self.center_form_anchors, self.center_variance, self.size_variance)
        return locations, labels