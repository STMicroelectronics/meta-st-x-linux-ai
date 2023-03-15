#!/bin/bash
WIDTH=$1
HEIGHT=$2
FPS=$3
DEVICE=$4
CAMERA_WIDTH=640
CAMERA_HEIGHT=480

is_dcmipp_present() {
    echo "is_dcmipp_present function"
    DCMIPP_SENSOR="NOTFOUND"
    # on disco board ov5640 camera can be present on csi connector
    for video in $(find /sys/class/video4linux -name "video*" -type l);
    do
        if [ "$(cat $video/name)" = "dcmipp_dump_capture" ]; then
            cd $video/device/
            mediadev=/dev/$(ls -d media*)
            cd -
            for sub in $(find /sys/class/video4linux -name "v4l-subdev*" -type l);
            do
                subdev_name=$(tr -d '\0' < $sub/name | awk '{print $1}')
                if [ "$subdev_name" = "gc2145" ] || [ "$subdev_name" = "ov5640" ]; then
                    DCMIPP_SENSOR=$subdev_name
                    V4L_DEVICE="$(basename $video)"
                    sensorsubdev=$(tr -d '\0' < $sub/name)
                    #bridge is connected to output of sensor (":0 [ENABLED" with media-ctl -p)
                    bridgesubdev=$(media-ctl -d $mediadev -p -e "$sensorsubdev" | grep ":0 \[ENABLED" | awk -F\" '{print $2}')
                    #interface is connected to input of postproc (":1 [ENABLED" with media-ctl -p)
                    interfacesubdev=$(media-ctl -d $mediadev -p -e "dcmipp_dump_postproc" | grep ":1 \[ENABLED" | awk -F\" '{print $2}')
                    return
                fi
            done
        fi
    done
}

get_webcam_device() {
    echo "get_webcam_device function"
    found="NOTFOUND"
    for video in $(find /sys/class/video4linux -name "video*" -type l | sort);
    do
        if [ "$(cat $video/name)" = "dcmipp_dump_capture" ]; then
            found="FOUND"
        else
            V4L_DEVICE="$(basename $video)"
            break;
        fi
    done
}

# ------------------------------
#         main
# ------------------------------

#if a video device is specified in the launch script use it if not search for
#dcmipp camera of a webcam
if [ "$DEVICE" != "" ]; then
    DCMIPP_SENSOR="NOTFOUND"
    if [ "$(cat /sys/class/video4linux/$DEVICE/name)" = "dcmipp_dump_capture" ]; then
	cd /sys/class/video4linux/$DEVICE/device/
        mediadev=/dev/$(ls -d media*)
        cd -
        for sub in $(find /sys/class/video4linux -name "v4l-subdev*" -type l);
        do
	    subdev_name=$(tr -d '\0' < $sub/name | awk '{print $1}')
            if [ "$subdev_name" = "gc2145" ] || [ "$subdev_name" = "ov5640" ]; then
                DCMIPP_SENSOR=$subdev_name
	        echo "DCMIPP_SENSOR="$DCMIPP_SENSOR
                V4L_DEVICE="$(basename $DEVICE)"
                sensorsubdev=$(tr -d '\0' < $sub/name)
                #bridge is connected to output of sensor (":0 [ENABLED" with media-ctl -p)
                bridgesubdev=$(media-ctl -d $mediadev -p -e "$sensorsubdev" | grep ":0 \[ENABLED" | awk -F\" '{print $2}')
                #interface is connected to input of postproc (":1 [ENABLED" with media-ctl -p)
                interfacesubdev=$(media-ctl -d $mediadev -p -e "dcmipp_dump_postproc" | grep ":1 \[ENABLED" | awk -F\" '{print $2}')
	   fi
        done
    fi
else
    is_dcmipp_present
fi

if [ "$DCMIPP_SENSOR" != "NOTFOUND" ]; then
    if [ "$DCMIPP_SENSOR" = "gc2145" ]; then
        sensorbuscode="RGB565_2X8_BE"
    elif [ "$DCMIPP_SENSOR" = "ov5640" ]; then
        sensorbuscode="RGB565_2X8_LE"
    fi
    media-ctl -d $mediadev --set-v4l2 "'$sensorsubdev':0[fmt:$sensorbuscode/${CAMERA_WIDTH}x${CAMERA_HEIGHT}@1/${FPS} field:none]"
    media-ctl -d $mediadev --set-v4l2 "'$bridgesubdev':2[fmt:$sensorbuscode/${CAMERA_WIDTH}x${CAMERA_HEIGHT}]"
    media-ctl -d $mediadev --set-v4l2 "'$interfacesubdev':1[fmt:RGB565_2X8_LE/${CAMERA_WIDTH}x${CAMERA_HEIGHT}]"
    media-ctl -d $mediadev --set-v4l2 "'dcmipp_dump_postproc':1[fmt:RGB565_2X8_LE/${CAMERA_WIDTH}x${CAMERA_HEIGHT}]"
    media-ctl -d $mediadev --set-v4l2 "'dcmipp_dump_postproc':0[compose: (0,0)/${WIDTH}x${HEIGHT}]"
    v4l2-ctl -d /dev/v4l-subdev6 --set-ctrl=horizontal_flip=1
    V4L2_CAPS="video/x-raw,format=RGB16"
    V4L_OPT=""
else
    get_webcam_device
    # suppose we have a webcam
    V4L2_CAPS="video/x-raw"
    V4L_OPT="io-mode=4"
    v4l2-ctl --set-parm=20
fi

echo "V4L_DEVICE="$V4L_DEVICE
echo "V4L2_CAPS="$V4L2_CAPS
