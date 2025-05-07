#!/bin/sh
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')
source /usr/local/x-linux-ai/resources/config_board.sh
cmd="python3 /usr/local/x-linux-ai/on-device-learning/odl_teacher_student_obj_detect.py -t /usr/local/x-linux-ai/on-device-learning/teacher_model/rt-detr/rtdetr-l.onnx -l /usr/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/labels.txt  --training_artifacts_path /usr/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/training_artifacts/"

if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
