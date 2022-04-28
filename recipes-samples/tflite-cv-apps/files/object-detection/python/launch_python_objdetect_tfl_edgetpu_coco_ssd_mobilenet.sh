#!/bin/sh
weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')

cmd="python3 /usr/local/demo-ai/computer-vision/tflite-object-detection/python/objdetect_tfl.py -m /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/detect_edgetpu.tflite -l /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/labels.txt -v 0 --edgetpu"
if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	su -l $weston_user -c "$cmd"
else
	$cmd
fi
