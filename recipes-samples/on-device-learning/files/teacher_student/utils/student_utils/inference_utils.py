#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import numpy as np
from utils.student_utils import postprocessing_utils
from utils.student_utils.postprocessing_utils import generate_ssd_anchors
from utils.dataset_utils.preprocessing_utils import InferenceTransform
import utils.student_utils.ssd_mobilenetv2_utils as config
import time

from stai_mpu import stai_mpu_network

#
class SSD_MobileNetV2:
    """
    Class to load an onnx model providing the required pre-processing and post-processing for SSD
    """
    def __init__(self, model_path, labels_file_path, conf_thres=0.01, iou_thres=0.5, hw_acceleration=False):
        self.labels_file_path = labels_file_path
        self.conf_threshold = conf_thres
        self.iou_threshold = iou_thres
        # Initialize model
        self.initialize_model(model_path, hw_acceleration)
        self.anchors = generate_ssd_anchors(config.specs, self.input_shape[2])

    def __call__(self, image):
        return self.get_results(image)

    def initialize_model(self, model_path, hw_acceleration):
        # Initialize NN model
        self.stai_mpu_model = stai_mpu_network(model_path=model_path, use_hw_acceleration=hw_acceleration)
        # Read input tensor information
        self.num_inputs = self.stai_mpu_model.get_num_inputs()
        self.input_tensor_infos = self.stai_mpu_model.get_input_infos()
        # Read output tensor information
        self.num_outputs = self.stai_mpu_model.get_num_outputs()
        self.output_tensor_infos = self.stai_mpu_model.get_output_infos()
        self.get_input_details()

    def get_labels(self):
        return [name.strip() for name in open(self.labels_file_path).readlines()]

    def get_results(self, image):
        input_tensor, preprocess_time = self.preprocess_input(image)
        scores, boxes, inference_time = self.run_inference(input_tensor)
        self.boxes, self.probs, self.labels, postprocessing_utils_time = self.postprocess_output(scores, boxes)
        return self.boxes, self.probs, self.labels, preprocess_time, inference_time, postprocessing_utils_time

    def preprocess_input(self, image):
        start_time = time.perf_counter()
        self.img_height, self.img_width = image.shape[:2]
        image = InferenceTransform(self.input_width, self.input_height, 127, 128.0)(image)
        input_tensor = np.expand_dims(image, axis=0)
        input_tensor = input_tensor.copy()
        end_time = (time.perf_counter() - start_time) * 1000  # ms
        return input_tensor, end_time

    def run_inference(self, input_tensor):
        self.stai_mpu_model.set_input(0, np.array(input_tensor))
        start_time = time.perf_counter()
        self.stai_mpu_model.run()
        inference_time = (time.perf_counter() - start_time) * 1000  # ms
        scores_tensor = self.stai_mpu_model.get_output(index=0)
        boxes_tensor = self.stai_mpu_model.get_output(index=1)
        return scores_tensor, boxes_tensor, inference_time

    def postprocess_output(self, scores, boxes):
        start_time = time.perf_counter()
        boxes, probs, picked_labels = process_output(scores, boxes, self.anchors, self.conf_threshold, self.iou_threshold, img_width=self.img_width, img_height=self.img_height)
        end_time = (time.perf_counter() - start_time) * 1000  # ms
        return boxes, probs, picked_labels, end_time

    def get_input_details(self):
        self.input_shape = self.input_tensor_infos[0].get_shape()
        self.input_height = self.input_shape[2]
        self.input_width = self.input_shape[3]
        self.input_channel = self.input_shape[1]

    def get_output_details(self):
        model_outputs = self.session.get_outputs()
        self.output_names = [
            model_outputs[i].name for i in range(len(model_outputs))]

    def get_label(self, idx, classes):
        labels = self.get_labels()
        return labels[int(classes[idx]) - 1]


def process_output(scores, boxes, anchors, conf_threshold=0.01, iou_threshold=0.5, img_width=300, img_height=300):
    """
    Generic post processing function, defined both in evaluation task and
    inference task, that's the reason the function is defined separately.
    """
    # Apply softmax to the scores
    # Subtract the maximum value for numerical stability
    scores = scores - np.max(scores, axis=2, keepdims=True)
    # Compute the softmax
    scores = np.exp(scores) / np.sum(np.exp(scores), axis=2, keepdims=True)
    boxes = postprocessing_utils.convert_locations_to_boxes(boxes, anchors, 0.1, 0.2)
    boxes = postprocessing_utils.center_form_to_corner_form(boxes)
    boxes = np.array(boxes[0])
    scores = np.array(scores[0])
    picked_box_probs = []
    picked_labels = []

    for class_index in range(1, scores.shape[1]):
        probs = scores[:, class_index]
        mask = probs > conf_threshold
        probs = probs[mask]
        if probs.shape[0] == 0:
            continue
        subset_boxes = boxes[mask, :]
        box_probs = np.concatenate([subset_boxes, probs.reshape(-1, 1)], axis=1)
        box_probs = postprocessing_utils.hard_nms(box_probs, iou_threshold=iou_threshold)
        picked_box_probs.append(box_probs)
        picked_labels.extend([class_index] * box_probs.shape[0])

    if not picked_box_probs:
        picked_box_probs = np.array([])
        boxes = np.empty((0, 4))
    else:
        picked_box_probs = np.concatenate(picked_box_probs)
        picked_box_probs[:, 0] *= img_width
        picked_box_probs[:, 1] *= img_height
        picked_box_probs[:, 2] *= img_width
        picked_box_probs[:, 3] *= img_height
        boxes = picked_box_probs[:, :4]
        probs = picked_box_probs[:, 4]

    return boxes, probs, picked_labels