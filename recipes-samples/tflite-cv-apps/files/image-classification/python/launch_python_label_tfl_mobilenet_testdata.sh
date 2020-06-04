#!/bin/sh
python3 /usr/local/demo-ai/computer-vision/tflite-image-classification/python/label_tfl_multiprocessing.py -m /usr/local/demo-ai/computer-vision/models/mobilenet/mobilenet_v1_0.5_128_quant.tflite -l /usr/local/demo-ai/computer-vision/models/mobilenet/labels.txt -i /usr/local/demo-ai/computer-vision/models/mobilenet/testdata/
