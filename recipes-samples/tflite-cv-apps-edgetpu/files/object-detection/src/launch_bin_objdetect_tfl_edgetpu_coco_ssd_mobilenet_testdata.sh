#!/bin/sh
/usr/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/objdetect_tfl_edgetpu_gst_gtk -m /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/detect_edgetpu.tflite -l /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/labels.txt -i /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/testdata/
