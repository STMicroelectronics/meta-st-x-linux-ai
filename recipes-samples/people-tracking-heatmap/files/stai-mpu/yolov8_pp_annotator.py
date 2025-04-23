#!/usr/bin/python3
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

from stai_mpu import stai_mpu_network
from timeit import default_timer as timer
from abc import ABC, abstractmethod
from typing import Optional, TypeVar
from PIL import Image
from supervision.detection.core import Detections
from supervision.geometry.core import Position
from supervision.utils.conversion import (
    ensure_cv2_image_for_annotation,
    ensure_pil_image_for_annotation,
)

import cv2
import numpy as np

ImageType = TypeVar("ImageType", np.ndarray, Image.Image)
"""
An image of type `np.ndarray` or `PIL.Image.Image`.

Unlike a `Union`, ensures the type remains consistent. If a function
takes an `ImageType` argument and returns an `ImageType`, when you
pass an `np.ndarray`, you will get an `np.ndarray` back.
"""

class NeuralNetwork:
    """
    Class that handles Neural Network inference
    """

    def __init__(self, model_file, input_mean, input_std, maximum_detection, det_threshold, iou_threshold):
        """
        :param model_path: .tflite model to be executed
        :param input_mean: input_mean
        :param input_std: input standard deviation
        """

        self._model_file = model_file
        print("NN model used : ", self._model_file)
        self._input_mean = input_mean
        self._input_std = input_std
        self.maximum_detection = maximum_detection
        self.threshold = det_threshold
        self.iou_threshold = iou_threshold

        # Initialize NN model
        self.stai_mpu_model = stai_mpu_network(model_path=self._model_file, use_hw_acceleration=True)

        # Read input tensor information
        self.num_inputs = self.stai_mpu_model.get_num_inputs()
        self.input_tensor_infos = self.stai_mpu_model.get_input_infos()

        # Read output tensor information
        self.num_outputs = self.stai_mpu_model.get_num_outputs()
        self.output_tensor_infos = self.stai_mpu_model.get_output_infos()

    def get_img_size(self):
        """
        :return: size of NN input image size
        """
        input_tensor_shape = self.input_tensor_infos[0].get_shape()
        print("input_tensor_shape",input_tensor_shape)
        input_width = input_tensor_shape[1]
        input_height = input_tensor_shape[2]
        input_channel = input_tensor_shape[3]
        return (input_width, input_height, input_channel)

    def launch_inference(self, img):
        """
        This method launches inference using the invoke call
        :param img: the image to be inferred
        """
        # add N dim
        input_data = np.expand_dims(img, axis=0)

        # preprocess input data if necessary
        if self.input_tensor_infos[0].get_dtype() == np.float32:
            input_data = (np.float32(input_data) - self._input_mean) / self._input_std

        # set NN model input with input data
        self.stai_mpu_model.set_input(0, input_data)
        start = timer()
        # run inference
        self.stai_mpu_model.run()
        end = timer()
        # return the inference time
        inference_time = end - start
        return inference_time

    def intersection(self, rect1, rect2):
        """
        This method return the intersection of two rectangles
        """
        rect1_x1,rect1_y1,rect1_x2,rect1_y2 = rect1[:4]
        rect2_x1,rect2_y1,rect2_x2,rect2_y2 = rect2[:4]
        x1 = max(rect1_x1,rect2_x1)
        y1 = max(rect1_y1,rect2_y1)
        x2 = min(rect1_x2,rect2_x2)
        y2 = min(rect1_y2,rect2_y2)
        return (x2-x1)*(y2-y1)

    def union(self, rect1,rect2):
        """
        This method return the union of two rectangles
        """
        rect1_x1,rect1_y1,rect1_x2,rect1_y2 = rect1[:4]
        rect2_x1,rect2_y1,rect2_x2,rect2_y2 = rect2[:4]
        rect1_area = (rect1_x2-rect1_x1)*(rect1_y2-rect1_y1)
        rect2_area = (rect2_x2-rect2_x1)*(rect2_y2-rect2_y1)
        return rect1_area + rect2_area - self.intersection(rect1,rect2)

    def iou(self, rect1,rect2):
        """
        This method compute IoU
        """
        return self.intersection(rect1,rect2)/self.union(rect1,rect2)

    def get_results(self):
         # Lists to hold respective values while unwrapping.
        base_objects_list = []
        final_dets = []

        # Output (0-4: box coordinates, 5-84: COCO classes confidence)
        output = self.stai_mpu_model.get_output(index=0)
        output = np.transpose(np.squeeze(output))

        # Split output -> [0..3]: box coordinates, [5]: confidence level
        confidence_level = output[:, 4:]  # Shape: (1344, 1)

        indices = np.where(confidence_level > self.threshold)[0]
        filtered_output = output[indices]

        for i in range(filtered_output.shape[0]):
            x_center, y_center, width, height = filtered_output[i][:4]
            left = (x_center - width/2)
            top = (y_center - height/2)
            right = (x_center + width/2)
            bottom = (y_center + height/2)
            score = filtered_output[i][4]
            class_id = 0
            base_objects_list.append([left, top, right, bottom, score, class_id])

        # Do NMS
        base_objects_list.sort(key=lambda x: x[4], reverse=True)
        while len(base_objects_list)>0:
            final_dets.append(base_objects_list[0])
            base_objects_list = [objects for objects in base_objects_list if self.iou(objects,base_objects_list[0]) < self.iou_threshold]

        return final_dets

class BaseAnnotator(ABC):
    @abstractmethod
    def annotate(self, scene: ImageType, detections: Detections) -> ImageType:
        pass

class BoxAnnotator(BaseAnnotator):
    """
    A class for drawing bounding boxes on an image using provided detections.
    """

    def __init__(
        self,
        thickness: int = 2,
    ):
        """
        Args:
            thickness (int): Thickness of the bounding box lines.
        """
        self.thickness: int = thickness

    @ensure_cv2_image_for_annotation
    def annotate(
        self,
        scene: ImageType,
        detections: Detections,
        color = (0, 255, 0, 255),
    ) -> ImageType:
        """
        Annotates the given scene with bounding boxes based on the provided detections.

        Args:
            scene (ImageType): The image where bounding boxes will be drawn. `ImageType`
                is a flexible type, accepting either `numpy.ndarray` or
                `PIL.Image.Image`.
            detections (Detections): Object detections to annotate.
            color: bouding box color (r, g, b, alpha)

        Returns:
            The annotated image, matching the type of `scene` (`numpy.ndarray`
                or `PIL.Image.Image`)

        Example:
            ```python
            import supervision as sv

            image = ...
            detections = sv.Detections(...)

            box_annotator = sv.BoxAnnotator()
            annotated_frame = box_annotator.annotate(
                scene=image.copy(),
                detections=detections
            )
            ```

        ![bounding-box-annotator-example](https://media.roboflow.com/
        supervision-annotator-examples/bounding-box-annotator-example-purple.png)
        """
        assert isinstance(scene, np.ndarray)
        for detection_idx in range(len(detections)):
            x1, y1, x2, y2 = detections.xyxy[detection_idx].astype(int)
            cv2.rectangle(
                img=scene,
                pt1=(x1, y1),
                pt2=(x2, y2),
                color=color,
                thickness=self.thickness,
            )
        return scene

class Trace:
    def __init__(
        self,
        max_size: Optional[int] = None,
        start_frame_id: int = 0,
        anchor: Position = Position.CENTER,
    ) -> None:
        self.current_frame_id = start_frame_id
        self.max_size = max_size
        self.anchor = anchor

        self.frame_id = np.array([], dtype=int)
        self.xy = np.empty((0, 2), dtype=np.float32)
        self.tracker_id = np.array([], dtype=int)

    def put(self, detections: Detections) -> None:
        frame_id = np.full(len(detections), self.current_frame_id, dtype=int)
        self.frame_id = np.concatenate([self.frame_id, frame_id])
        self.xy = np.concatenate(
            [self.xy, detections.get_anchors_coordinates(self.anchor)]
        )
        self.tracker_id = np.concatenate([self.tracker_id, detections.tracker_id])

        unique_frame_id = np.unique(self.frame_id)

        if 0 < self.max_size < len(unique_frame_id):
            max_allowed_frame_id = self.current_frame_id - self.max_size + 1
            filtering_mask = self.frame_id >= max_allowed_frame_id
            self.frame_id = self.frame_id[filtering_mask]
            self.xy = self.xy[filtering_mask]
            self.tracker_id = self.tracker_id[filtering_mask]

        self.current_frame_id += 1

    def get(self, tracker_id: int) -> np.ndarray:
        return self.xy[self.tracker_id == tracker_id]

class TraceAnnotator(BaseAnnotator):
    """
    A class for drawing trace paths on an image based on detection coordinates.

    !!! warning

        This annotator uses the `sv.Detections.tracker_id`. Read
        [here](/latest/trackers/) to learn how to plug
        tracking into your inference pipeline.
    """

    def __init__(
        self,
        position: Position = Position.CENTER,
        trace_length: int = 30,
        thickness: int = 2,
    ):
        """
        Args:
            position (Position): The position of the trace.
                Defaults to `CENTER`.
            trace_length (int): The maximum length of the trace in terms of historical
                points. Defaults to `30`.
            thickness (int): The thickness of the trace lines. Defaults to `2`.
        """
        self.trace = Trace(max_size=trace_length, anchor=position)
        self.thickness = thickness

    @ensure_cv2_image_for_annotation
    def annotate(
        self,
        scene: ImageType,
        detections: Detections,
        color = (0, 255, 0, 255),
    ) -> ImageType:
        """
        Draws trace paths on the frame based on the detection coordinates provided.

        Args:
            scene (ImageType): The image on which the traces will be drawn.
                `ImageType` is a flexible type, accepting either `numpy.ndarray`
                or `PIL.Image.Image`.
            detections (Detections): The detections which include coordinates for
                which the traces will be drawn.
            color: trace color (r, g, b, alpha)

        Returns:
            The annotated image, matching the type of `scene` (`numpy.ndarray`
                or `PIL.Image.Image`)

        Example:
            ```python
            import supervision as sv
            from ultralytics import YOLO

            model = YOLO('yolov8x.pt')
            trace_annotator = sv.TraceAnnotator()

            video_info = sv.VideoInfo.from_video_path(video_path='...')
            frames_generator = sv.get_video_frames_generator(source_path='...')
            tracker = sv.ByteTrack()

            with sv.VideoSink(target_path='...', video_info=video_info) as sink:
               for frame in frames_generator:
                   result = model(frame)[0]
                   detections = sv.Detections.from_ultralytics(result)
                   detections = tracker.update_with_detections(detections)
                   annotated_frame = trace_annotator.annotate(
                       scene=frame.copy(),
                       detections=detections)
                   sink.write_frame(frame=annotated_frame)
            ```

        ![trace-annotator-example](https://media.roboflow.com/
        supervision-annotator-examples/trace-annotator-example-purple.png)
        """
        assert isinstance(scene, np.ndarray)
        if detections.tracker_id is None:
            raise ValueError(
                "The `tracker_id` field is missing in the provided detections."
                " See more: https://supervision.roboflow.com/latest/how_to/track_objects"
            )

        self.trace.put(detections)
        for detection_idx in range(len(detections)):
            tracker_id = int(detections.tracker_id[detection_idx])
            xy = self.trace.get(tracker_id=tracker_id)
            if len(xy) > 1:
                scene = cv2.polylines(
                    scene,
                    [xy.astype(np.int32)],
                    False,
                    color=color,
                    thickness=self.thickness,
                )
        return scene