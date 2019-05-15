#!/bin/sh
python3 /usr/local/demo-ai/python/label_tfl_multiprocessing.py -m /usr/local/demo-ai/models/mobilenet/mobilenet_v1_0.5_128_quant.tflite -l /usr/local/demo-ai/models/mobilenet/labels.txt -v 0
