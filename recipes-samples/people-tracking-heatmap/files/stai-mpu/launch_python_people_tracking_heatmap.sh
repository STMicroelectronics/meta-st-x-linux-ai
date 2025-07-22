#!/bin/sh
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

/usr/sbin/stm32_usbotg_eth_config.sh stop
/usr/sbin/stm32_usbotg_acm_uvc_config.sh start 1280 720 29
weston_user=$(ps aux | grep '/usr/bin/weston '|grep -v 'grep'|awk '{print $1}')
cmd="python3 /usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py -m /usr/local/x-linux-ai/people-tracking-heatmap/models/yolov8n_people/yolov8n_320_quant_pt_uf_od_coco-person-st.nb --frame_width 1280 --frame_height 720"
if [ "$weston_user" != "root" ]; then
	echo "user : "$weston_user
	script -qc "su -l $weston_user -c '$cmd'"
else
	$cmd
fi