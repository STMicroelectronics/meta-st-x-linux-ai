#!/bin/sh
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')
source /usr/local/demo-ai/computer-vision/onnx-object-detection/python/resources/config_board.sh
cmd="python3 /usr/local/demo-ai/computer-vision/onnx-object-detection/python/objdetect_onnx.py -m /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/detect.onnx -l /usr/local/demo-ai/computer-vision/models/coco_ssd_mobilenet/labels_onnx.txt --framerate $DFPS --frame_width $DWIDTH --frame_height $DHEIGHT $COMPUTE_ENGINE"
if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
