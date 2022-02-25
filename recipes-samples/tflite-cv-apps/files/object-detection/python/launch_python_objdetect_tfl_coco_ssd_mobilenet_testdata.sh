#!/bin/sh
python3 /usr/local/demo-ai/computer-vision/tflite-object-detection/python/objdetect_tfl.py -m /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/detect.tflite -l /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/labels.txt -i /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/testdata/
