#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

from collections import Counter
import numpy as np
from utils.student_utils import postprocessing_utils
import re

def compute_mAP(precisions, recalls):
    """
    Computes the mean average precision based on the definition of the Pascal Competition.
    It calculates the area under the precision-recall curve.
    Recall follows the standard definition.
    Precision is adjusted as follows:
    pascal_precision[i] = typical_precision[i:].max()
    """
    # Extend precisions and recalls with boundary values
    precisions = np.concatenate([[0.0], precisions, [0.0]])
    for i in range(len(precisions) - 1, 0, -1):
        precisions[i - 1] = np.maximum(precisions[i - 1], precisions[i])

    # Extend recalls with boundary values
    recalls = np.concatenate([[0.0], recalls, [1.0]])
    change_indices = np.where(recalls[1:] != recalls[:-1])[0]

    # Calculate the area under the curve
    areas = (recalls[change_indices + 1] - recalls[change_indices]) * precisions[change_indices + 1]
    return areas.sum()

def group_annotation_by_class(dataset):
    true_case_stat = {}
    all_gt_boxes = {}
    all_difficult_cases = {}
    for i in range(len(dataset)):
        image_id, annotation = dataset.get_annotation(i)
        gt_boxes, gt_classes, is_difficult = annotation
        gt_boxes = np.array(gt_boxes)
        for i, difficult in enumerate(is_difficult):
            class_index = int(gt_classes[i])
            gt_box = gt_boxes[i]
            if not difficult:
                true_case_stat[class_index] = true_case_stat.get(class_index, 0) + 1
            if class_index not in all_gt_boxes:
                all_gt_boxes[class_index] = {}
            if image_id not in all_gt_boxes[class_index]:
                all_gt_boxes[class_index][image_id] = []
            all_gt_boxes[class_index][image_id].append(gt_box)
            if class_index not in all_difficult_cases:
                all_difficult_cases[class_index] = {}
            if image_id not in all_difficult_cases[class_index]:
                all_difficult_cases[class_index][image_id] = []
            all_difficult_cases[class_index][image_id].append(difficult)
    for class_index in all_gt_boxes:
        for image_id in all_gt_boxes[class_index]:
            all_gt_boxes[class_index][image_id] = np.stack(all_gt_boxes[class_index][image_id])
    for class_index in all_difficult_cases:
        for image_id in all_difficult_cases[class_index]:
            all_difficult_cases[class_index][image_id] = np.array(all_difficult_cases[class_index][image_id])
    return true_case_stat, all_gt_boxes, all_difficult_cases


def compute_mAP_per_class(num_ground_truths, ground_truth_boxes, difficult_cases,
                          prediction_file_path, iou_threshold):
    with open(prediction_file_path) as file:
        image_identifiers = []
        predicted_boxes = []
        confidence_scores = []
        for line in file:
            tokens = line.rstrip().split(" ")
            image_identifiers.append(tokens[0])
            # Extract the numeric value from the tensor string
            if 'copy' in tokens:
                tokens.remove('copy')
            score_str = re.findall(r"[-+]?\d*\.\d+|\d+", tokens[1])
            if score_str:
                confidence_scores.append(float(score_str[0]))
            else:
                raise ValueError(f"Could not extract a float from the string '{tokens[1]}'")
            # Extract the bounding box coordinates
            box_coordinates = [float(re.findall(r"[-+]?\d*\.\d+|\d+", coord)[0]) for coord in tokens[2:]]
            box = np.array(box_coordinates).reshape(1, -1)
            box -= 1.0  # convert to python format where indexes start from 0
            predicted_boxes.append(box)
        confidence_scores = np.array(confidence_scores)
        sorted_indices = np.argsort(-confidence_scores)
        predicted_boxes = [predicted_boxes[i] for i in sorted_indices]
        image_identifiers = [image_identifiers[i] for i in sorted_indices]
        true_positives = np.zeros(len(image_identifiers))
        false_positives = np.zeros(len(image_identifiers))
        matched_boxes = set()
        for i, image_id in enumerate(image_identifiers):
            box = predicted_boxes[i]
            if image_id not in ground_truth_boxes:
                false_positives[i] = 1
                continue

            gt_box = ground_truth_boxes[image_id]
            ious = postprocessing_utils.iou_of(box, gt_box)
            max_iou = np.max(ious)
            max_index = np.argmax(ious)
            if max_iou > iou_threshold:
                if difficult_cases[image_id][max_index] == 0:
                    if (image_id, max_index) not in matched_boxes:
                        true_positives[i] = 1
                        matched_boxes.add((image_id, max_index))
                    else:
                        false_positives[i] = 1
            else:
                false_positives[i] = 1

    true_positives = np.cumsum(true_positives)
    false_positives = np.cumsum(false_positives)
    precisions = true_positives / (true_positives + false_positives)
    recalls = true_positives / num_ground_truths

    return compute_mAP(precisions, recalls)
