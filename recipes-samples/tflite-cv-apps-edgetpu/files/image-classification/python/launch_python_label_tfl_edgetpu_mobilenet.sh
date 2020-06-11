#!/bin/sh
python3 /usr/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/python/label_tfl_edgetpu.py -m /usr/local/demo-ai/computer-vision/models/mobilenet/mobilenet_v1_1.0_224_quant_edgetpu.tflite -l /usr/local/demo-ai/computer-vision/models/mobilenet/labels.txt
