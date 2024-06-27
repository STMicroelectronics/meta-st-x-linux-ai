#!/bin/sh
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')
FRAMEWORK=$1
echo "stai_mpu wrapper used : "$FRAMEWORK
source /usr/local/x-linux-ai/resources/config_board.sh
cmd="/usr/local/x-linux-ai/object-detection/stai_mpu_object_detection -m /usr/local/x-linux-ai/object-detection/models/$OBJ_DETEC_MODEL -l /usr/local/x-linux-ai/object-detection/models/$OBJ_DETEC_MODEL_LABEL.txt -i /usr/local/x-linux-ai/object-detection/models/$OBJ_DETECT_DATA"

if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
