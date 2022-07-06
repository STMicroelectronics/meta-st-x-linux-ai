#!/bin/sh
weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')

cmd="/usr/local/demo-ai/computer-vision/tflite-image-classification/bin/label_tfl_gst_gtk  -m /usr/local/demo-ai/computer-vision/models/mobilenet/mobilenet_v1_0.5_128_quant.tflite -l /usr/local/demo-ai/computer-vision/models/mobilenet/labels.txt -i /usr/local/demo-ai/computer-vision/models/mobilenet/testdata/"
if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
