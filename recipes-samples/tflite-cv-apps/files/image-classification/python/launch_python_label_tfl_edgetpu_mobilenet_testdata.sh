#!/bin/sh
python3 /usr/local/demo-ai/computer-vision/tflite-image-classification/python/label_tfl.py -m /usr/local/demo-ai/computer-vision/models/mobilenet/mobilenet_v1_1.0_224_quant_edgetpu.tflite -l /usr/local/demo-ai/computer-vision/models/mobilenet/labels.txt -i /usr/local/demo-ai/computer-vision/models/mobilenet/testdata/ --edgetpu
