#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import numpy as np
import logging
import xml.etree.ElementTree as ET
import cv2
import os
import glob
from pathlib import Path
import random
import shutil

class CustomDataset:
    def __init__(self, root, transform=None, target_transform=None, dataset_type='train', keep_difficult=False, labels_file=None):
        """ Class for custom dataset.
        Args:
            root: the root of the dataset, the directory contains the following sub-directories:
                Annotations, eval, test, train, and old_images. It also contains a labels.txt file containing the list of classes.
            dataset_type: a string indicating the type of dataset to use ('test', 'eval', 'train', 'old_images').
        """
        self.root = root
        self.transform = transform
        self.target_transform = target_transform
        self.dataset_type = dataset_type
        self.keep_difficult = keep_difficult

        # Validate dataset_type
        valid_types = ['test', 'eval', 'train', 'old_images']
        if self.dataset_type not in valid_types:
            raise ValueError(f"Invalid dataset_type '{self.dataset_type}'. Must be one of {valid_types}.")

        # Load image IDs based on dataset_type
        files = glob.glob(os.path.join(self.root, self.dataset_type, "*.[jp][pn]g"))
        if self.dataset_type in ['test', 'train'] and (len(files) - 1) % 4 == 0:
            files = files[:-1]
        self.ids = [Path(file).stem for file in files]

        # Load class names
        if os.path.isfile(labels_file):
            class_string = ""
            with open(labels_file, 'r') as infile:
                for line in infile:
                    class_string += line.rstrip()

            classes = class_string.split(',')
            classes.insert(0, 'BACKGROUND')
            classes = [elem.replace(" ", "") for elem in classes]
            self.class_names = tuple(classes)
            logging.info("Labels read from file: " + str(self.class_names))
        else:
            logging.info("No labels file, using default VOC classes.")
            self.class_names = ('BACKGROUND',
                                'aeroplane', 'bicycle', 'bird', 'boat',
                                'bottle', 'bus', 'car', 'cat', 'chair',
                                'cow', 'diningtable', 'dog', 'horse',
                                'motorbike', 'person', 'pottedplant',
                                'sheep', 'sofa', 'train', 'tvmonitor')

        self.class_dict = {class_name: i for i, class_name in enumerate(self.class_names)}

    def __getitem__(self, index):
        image_id = self.ids[index]
        image = self._read_image(image_id)
        boxes, labels, is_difficult = self._get_annotation(image_id)
        if not self.keep_difficult:
            boxes = boxes[is_difficult == 0]
            labels = labels[is_difficult == 0]
        if self.transform:
            image, boxes, labels = self.transform(image, boxes, labels)
        if self.target_transform:
            boxes, labels = self.target_transform(boxes, labels)
        return image, boxes, labels

    def get_image(self, index):
        image_id = self.ids[index]
        image = self._read_image(image_id)
        if self.transform:
            image, _ = self.transform(image)
        return image

    def get_image_with_name(self, index):
        image_id = self.ids[index]
        image = self._read_image(image_id)
        if self.transform:
            image, _ = self.transform(image)
        return image, image_id

    def get_annotation(self, index):
        image_id = self.ids[index]
        return image_id, self._get_annotation(image_id)

    def __len__(self):
        return len(self.ids)

    def _get_annotation(self, image_id):
        annotation_file = self.root + f"Annotations/{image_id}.xml"
        objects = ET.parse(annotation_file).findall("object")
        boxes = []
        labels = []
        is_difficult = []
        for object in objects:
            class_name = object.find('name').text.lower().strip()
            if class_name in self.class_dict:
                bbox = object.find('bndbox')
                x1 = float(bbox.find('xmin').text) - 1
                y1 = float(bbox.find('ymin').text) - 1
                x2 = float(bbox.find('xmax').text) - 1
                y2 = float(bbox.find('ymax').text) - 1
                boxes.append([x1, y1, x2, y2])
                labels.append(self.class_dict[class_name])
                is_difficult_str = object.find('difficult').text
                is_difficult.append(int(is_difficult_str) if is_difficult_str else 0)
        return (np.array(boxes, dtype=np.float32),
                np.array(labels, dtype=np.int64),
                np.array(is_difficult, dtype=np.uint8))

    def _read_image(self, image_id):
        image_file = self.root + f"{self.dataset_type}/{image_id}.jpg" if os.path.exists(self.root + f"{self.dataset_type}/{image_id}.jpg") else self.root + f"{self.dataset_type}/{image_id}.png"
        image = cv2.imread(str(image_file))
        image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        return image

def split_data(dataset_dir, new_images_dir, old_images_dir, percentage_split_scale, old_images_scale, data_split_button):
        # Create dataset directory and subdirectories
        dataset_dir = dataset_dir

        # Split new_images into train, test, and eval
        new_image_files = [
            os.path.join(new_images_dir, f)
            for f in os.listdir(new_images_dir)
            if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.gif'))
        ]

        random.shuffle(new_image_files)
        total_new_images = len(new_image_files)
        train_ratio = int(percentage_split_scale.get_value() / 100 * total_new_images)
        # Data used for validation during training
        test_ratio = int(0.7 * (total_new_images - train_ratio))
        #  if test_ratio is one more than a multiple of 4, it is rounded down to the nearest multiple of 4.
        if (test_ratio - 1) % 4 == 0:
            test_ratio -=  1
        #  if train_ratio is one more than a multiple of 4, it is rounded down to the nearest multiple of 4.
        if (train_ratio - 1) % 4 == 0:
            train_ratio -= 1

        train_images = new_image_files[:train_ratio]
        test_images = new_image_files[train_ratio:train_ratio + test_ratio]
        eval_images = new_image_files[train_ratio + test_ratio:]

        # Move images to respective subfolders
        for image_path in train_images:
            shutil.move(image_path, os.path.join(dataset_dir, "train", os.path.basename(image_path)))
        for image_path in test_images:
            shutil.move(image_path, os.path.join(dataset_dir, "test", os.path.basename(image_path)))
        for image_path in eval_images:
            shutil.move(image_path, os.path.join(dataset_dir, "eval", os.path.basename(image_path)))

        # Copy old_images into train subfolder based on percentage
        old_images_dir = old_images_dir
        old_image_files = [
            os.path.join(old_images_dir, f)
            for f in os.listdir(old_images_dir)
            if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.gif'))
        ]

        # Calculate the number of old images to include
        percentage = int(old_images_scale.get_value())
        total_train_images = len(train_images)
        num_old_images = int((percentage / (100 - percentage)) * total_train_images) if percentage < 100 else len(old_image_files)

        # Ensure we don't exceed available old images
        num_old_images = min(num_old_images, len(old_image_files))
        selected_old_images = random.sample(old_image_files, num_old_images)
        for image_path in selected_old_images:
            shutil.copy(image_path, os.path.join(dataset_dir, "train", os.path.basename(image_path)))
        data_split_button.set_sensitive(False)

        print("Dataset preparation complete.")