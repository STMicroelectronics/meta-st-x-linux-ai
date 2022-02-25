#!/bin/sh
python3 /usr/local/demo-ai/computer-vision/tflite-image-classification/python/label_tfl.py -m /usr/local/demo-ai/computer-vision/models/mobilenet/mobilenet_v1_0.5_128_quant.tflite -l /usr/local/demo-ai/computer-vision/models/mobilenet/labels.txt -v 0
