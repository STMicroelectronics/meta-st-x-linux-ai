#!/bin/sh
python3 /usr/local/demo-ai/ai-cv/python/label_tfl_multiprocessing.py -m /usr/local/demo-ai/ai-cv/models/mobilenet/mobilenet_v1_0.5_128_quant.tflite -l /usr/local/demo-ai/ai-cv/models/mobilenet/labels.txt -v 0
