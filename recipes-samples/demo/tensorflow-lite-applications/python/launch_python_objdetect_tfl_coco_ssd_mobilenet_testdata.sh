#!/bin/sh
python3 /usr/local/demo-ai/python/objdetect_tfl_multiprocessing.py -m /usr/local/demo-ai/models/coco_ssd_mobilenet/detect.tflite -l /usr/local/demo-ai/models/coco_ssd_mobilenet/labels.txt -i /usr/local/demo-ai/models/coco_ssd_mobilenet/testdata/
