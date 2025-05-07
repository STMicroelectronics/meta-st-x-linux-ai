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
from onnxruntime import InferenceSession
import utils.teacher_utils.drawing_utils
import time

import xml.etree.ElementTree as ET
from xml.dom import minidom

# Class to load an onnx model providing the required pre-processing and post-processing for RTDETR model
class RTDETR:
    def __init__(self, path, conf_thres=0.5, target_labels: list = None):
        self.conf_threshold = conf_thres
        # Attribute used to filter results according to a particular label (e.g. keeping only persons from coco classes...)
        self.target_labels = target_labels
        # Initialize model
        self.initialize_model(path)

    def __call__(self, image):
        return self.detect_objects(image)

    def initialize_model(self, path):
        self.session = InferenceSession(path, providers=['CPUExecutionProvider'])
        self.get_input_details()
        self.get_output_details()

    def detect_objects(self, image):
        # Preprocess the input image
        input_tensor, preprocess_time = self.preprocess_input(image)
        # Run the inference
        outputs, inference_time = self.inference(input_tensor)
        # Post process the output tensors
        self.boxes, self.scores, self.class_ids, postprocessing_utils_time = self.postprocess_output(
            outputs, target_labels=self.target_labels)

        return self.boxes, self.scores, self.class_ids, preprocess_time, inference_time, postprocessing_utils_time

    def preprocess_input(self, image):
        # Measure preprocessing time
        start_time = time.perf_counter()
        self.img_height, self.img_width, self.img_depth = image.shape
        input_img = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        # Resize input image
        input_img = cv2.resize(input_img, (self.input_width, self.input_height))
        # Scale input pixel values to 0 to 1
        input_img = input_img / 255.0
        input_img = input_img.transpose(2, 0, 1)
        input_tensor = input_img[np.newaxis, :, :, :].astype(np.float32)
        end_time = (time.perf_counter() - start_time) * 1000  # ms
        return input_tensor, end_time

    def inference(self, input_tensor):
        # Measure inference time
        start_time = time.perf_counter()
        # Run the inference
        outputs = self.session.run(
            self.output_names, {self.input_names[0]: input_tensor})
        end_time = (time.perf_counter() - start_time) * 1000  # ms
        return outputs, end_time

    def postprocess_output(self, output, target_labels=None):
        # Measure postprocessing_utils time
        start_time = time.perf_counter()
        output = np.array(output[0])[0]
        boxes, scores = output[:, :4], output[:, 4:]
        # scores
        if not (np.all((scores > 0) & (scores < 1))):
            scores = 1 / (1 + np.exp(-scores))
        boxes = self.bbox_cxcywh_to_xyxy(boxes)
        _max = scores.max(-1)
        _mask = _max > self.conf_threshold
        boxes, scores = boxes[_mask], scores[_mask]
        labels, scores = scores.argmax(-1), scores.max(-1)

        # As the model is pretrained on several classes, it may detect unwanted classes
        # target_labels allows to select the classes of interest
        if target_labels is not None:
            # Filter by target label
            target_label_indices = [i for i, label in enumerate(
                labels) if label in target_labels]
            labels = np.array([labels[i] for i in target_label_indices])
            boxes = np.array([boxes[i] for i in target_label_indices])
            scores = np.array([scores[i] for i in target_label_indices])

        if scores.shape[0] == 0:
            return [], [], [], (time.perf_counter() - start_time) * 1000

        x1, y1, x2, y2 = boxes[:, 0], boxes[:, 1], boxes[:, 2], boxes[:, 3]
        x1 = np.floor(np.minimum(np.maximum(1, x1 * self.img_width), self.img_width - 1)).astype(int)
        y1 = np.floor(np.minimum(np.maximum(1, y1 * self.img_height), self.img_height - 1)).astype(int)
        x2 = np.ceil(np.minimum(np.maximum(1, x2 * self.img_width), self.img_width - 1)).astype(int)
        y2 = np.ceil(np.minimum(np.maximum(1, y2 * self.img_height), self.img_height - 1)).astype(int)
        boxes = np.stack([x1, y1, x2, y2], axis=1)
        end_time = (time.perf_counter() - start_time) * 1000  # ms

        return boxes, scores, labels, end_time

    def bbox_cxcywh_to_xyxy(self, boxes):
        cx, cy, w, h = boxes[:, 0], boxes[:, 1], boxes[:, 2], boxes[:, 3]
        x1 = cx - 0.5 * w
        y1 = cy - 0.5 * h
        x2 = cx + 0.5 * w
        y2 = cy + 0.5 * h
        return np.stack([x1, y1, x2, y2], axis=1)

    def xywh2xyxy(self, x):
        # Convert bounding box (x, y, w, h) to bounding box (x1, y1, x2, y2)
        y = np.copy(x)
        y[..., 0] = x[..., 0] - x[..., 2] / 2
        y[..., 1] = x[..., 1] - x[..., 3] / 2
        y[..., 2] = x[..., 0] + x[..., 2] / 2
        y[..., 3] = x[..., 1] + x[..., 3] / 2
        return y

    def extract_boxes(self, predictions):
        boxes = predictions[:, :4] # Extract boxes from predictions
        boxes = self.rescale_boxes(boxes) # Scale boxes to original image dimensions
        boxes = self.xywh2xyxy(boxes)  # Convert boxes to xyxy format
        return boxes

    def rescale_boxes(self, boxes):
        # Rescale boxes to original image dimensions
        input_shape = np.array(
            [self.input_width, self.input_height, self.input_width, self.input_height])
        boxes = np.divide(boxes, input_shape, dtype=np.float32)
        boxes *= np.array([self.img_width, self.img_height,
                          self.img_width, self.img_height])
        return boxes

    def get_input_details(self):
        model_inputs = self.session.get_inputs()
        self.input_names = [
            model_inputs[i].name for i in range(len(model_inputs))]
        self.input_shape = model_inputs[0].shape
        self.input_height = self.input_shape[2]
        self.input_width = self.input_shape[3]

    def get_output_details(self):
        model_outputs = self.session.get_outputs()
        self.output_names = [
            model_outputs[i].name for i in range(len(model_outputs))]

    def draw_detections(self, image, draw_scores=True, mask_alpha=0.4, bb_images: bool=False):
        return utils.teacher_utils.drawing_utils.draw_detections(image, self.boxes, self.scores,
                                           self.class_ids, mask_alpha, bb_images=bb_images)

    def format_predictions_for_xml(self, nb_detections, class_names, boxes):
        predictions = []
        for i in range(nb_detections):
            name = class_names[0]
            bbox = boxes[i]
            predictions.append({
                'name': name,
                'xmin': bbox[0],
                'xmax': bbox[2],
                'ymin': bbox[1],
                'ymax': bbox[3]
            })
        return predictions

    def generate_voc_xml_annotation(self, width, height, depth, objects, output_file, filename="NA", path="NA"):
        """
        Create an XML file in VOC format.
        :param filename: Name of the image file
        :param path: Path to the image file
        :param width: Width of the image
        :param height: Height of the image
        :param depth: Depth of the image (number of channels)
        :param objects: List of objects, where each object is a dictionary with keys:
                        'name', 'xmin', 'xmax', 'ymin', 'ymax'
        :param output_file: Path to the output XML file
        """
        annotation = ET.Element("annotation")
        folder = ET.SubElement(annotation, "folder")
        folder.text = ""
        filename_elem = ET.SubElement(annotation, "filename")
        filename_elem.text = filename
        path_elem = ET.SubElement(annotation, "path")
        path_elem.text = path
        source = ET.SubElement(annotation, "source")
        database = ET.SubElement(source, "database")
        database.text = "ST"
        size = ET.SubElement(annotation, "size")
        width_elem = ET.SubElement(size, "width")
        width_elem.text = str(width)
        height_elem = ET.SubElement(size, "height")
        height_elem.text = str(height)
        depth_elem = ET.SubElement(size, "depth")
        depth_elem.text = str(depth)
        segmented = ET.SubElement(annotation, "segmented")
        segmented.text = "0"
        for obj in objects:
            object_elem = ET.SubElement(annotation, "object")
            name = ET.SubElement(object_elem, "name")
            name.text = obj['name']
            pose = ET.SubElement(object_elem, "pose")
            pose.text = "Unspecified"
            truncated = ET.SubElement(object_elem, "truncated")
            truncated.text = "0"
            difficult = ET.SubElement(object_elem, "difficult")
            difficult.text = "0"
            occluded = ET.SubElement(object_elem, "occluded")
            occluded.text = "0"
            bndbox = ET.SubElement(object_elem, "bndbox")
            xmin = ET.SubElement(bndbox, "xmin")
            xmin.text = str(obj['xmin'])
            xmax = ET.SubElement(bndbox, "xmax")
            xmax.text = str(obj['xmax'])
            ymin = ET.SubElement(bndbox, "ymin")
            ymin.text = str(obj['ymin'])
            ymax = ET.SubElement(bndbox, "ymax")
            ymax.text = str(obj['ymax'])
        # Create a new XML file with the results
        tree = ET.ElementTree(annotation)
        tree.write(output_file)
        # Pretty print the XML
        xml_str = minidom.parseString(ET.tostring(
            annotation)).toprettyxml(indent="    ")
        with open(output_file, "w") as f:
            f.write(xml_str)

COCO_CLASSES = {
    "person": 0,
    "bicycle": 1,
    "car": 2,
    "motorbike": 3,
    "aeroplane": 4,
    "bus": 5,
    "train": 6,
    "truck": 7,
    "boat": 8,
    "traffic light": 9,
    "fire hydrant": 10,
    "stop sign": 11,
    "parking meter": 12,
    "bench": 13,
    "bird": 14,
    "cat": 15,
    "dog": 16,
    "horse": 17,
    "sheep": 18,
    "cow": 19,
    "elephant": 20,
    "bear": 21,
    "zebra": 22,
    "giraffe": 23,
    "backpack": 24,
    "umbrella": 25,
    "handbag": 26,
    "tie": 27,
    "suitcase": 28,
    "frisbee": 29,
    "skis": 30,
    "snowboard": 31,
    "sports ball": 32,
    "kite": 33,
    "baseball bat": 34,
    "baseball glove": 35,
    "skateboard": 36,
    "surfboard": 37,
    "tennis racket": 38,
    "bottle": 39,
    "wine glass": 40,
    "cup": 41,
    "fork": 42,
    "knife": 43,
    "spoon": 44,
    "bowl": 45,
    "banana": 46,
    "apple": 47,
    "sandwich": 48,
    "orange": 49,
    "broccoli": 50,
    "carrot": 51,
    "hot dog": 52,
    "pizza": 53,
    "donut": 54,
    "cake": 55,
    "chair": 56,
    "sofa": 57,
    "pottedplant": 58,
    "bed": 59,
    "diningtable": 60,
    "toilet": 61,
    "tvmonitor": 62,
    "laptop": 63,
    "mouse": 64,
    "remote": 65,
    "keyboard": 66,
    "cell phone": 67,
    "microwave": 68,
    "oven": 69,
    "toaster": 70,
    "sink": 71,
    "refrigerator": 72,
    "book": 73,
    "clock": 74,
    "vase": 75,
    "scissors": 76,
    "teddy bear": 77,
    "hair drier": 78,
    "toothbrush": 79
}