#!/bin/sh
weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')
source /usr/local/demo-ai/computer-vision/onnx-object-detection/bin/resources/config_board.sh
cmd="/usr/local/demo-ai/computer-vision/onnx-object-detection/bin/objdetect_onnx_gst_gtk -m /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/detect.onnx -l /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/labels.txt -i /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/testdata/ $COMPUTE_ENGINE"
if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
