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
echo "stai wrapper used : "$FRAMEWORK
CONFIG=$(find /usr/local/x-linux-ai -name "config_board_*.sh")
source $CONFIG
cmd="/usr/local/x-linux-ai/image-classification/stai_mpu_image_classification -m /usr/local/x-linux-ai/image-classification/models/$IMAGE_CLASSIFICATION_MODEL -l /usr/local/x-linux-ai/image-classification/models/$IMAGE_CLASSIFICATION_LABEL.txt -i /usr/local/x-linux-ai/image-classification/models/$IMAGE_CLASSIF_DATA"

if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
