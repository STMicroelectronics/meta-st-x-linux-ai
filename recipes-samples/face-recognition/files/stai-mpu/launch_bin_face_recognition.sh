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
source /usr/local/x-linux-ai/resources/config_board_npu.sh
cmd="/usr/local/x-linux-ai/face-recognition/stai_mpu_face_recognition -m /usr/local/x-linux-ai/face-recognition/models/$FACE_DETECTION_MODEL -f /usr/local/x-linux-ai/face-recognition/models/$FACE_RECO_MODEL -d /usr/local/x-linux-ai/face-recognition/$FACE_DATABASE --framerate $DFPS --frame_width $DWIDTH --frame_height $DHEIGHT"

if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi
