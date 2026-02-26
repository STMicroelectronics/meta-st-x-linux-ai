#!/bin/sh
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under MIT License terms that can be found here:
#
# https://opensource.org/license/mit

# This script configures USB Gadget configfs to use USB OTG as a USB serial
# gadget

configfs="/sys/kernel/config/usb_gadget"
g=g1
c=c.1
d="${configfs}/${g}"
func_acm=acm.GS0
func_uvc=uvc.0

VENDOR_ID="0x483"
PRODUCT_ID="0x5740"

do_start() {
    if [ ! -d ${configfs} ]; then
        modprobe libcomposite
        if [ ! -d ${configfs} ]; then
        exit 1
        fi
    fi

    if [ -d ${d} ]; then
        exit 0
    fi

    udc=$(ls -1 /sys/class/udc/)
    if [ -z $udc ]; then
        echo "No UDC driver registered"
        exit 1
    fi

    mkdir ${d}
    echo ${VENDOR_ID} > ${d}/idVendor
    echo ${PRODUCT_ID} > ${d}/idProduct

    # Both interface may not operate independently
    echo 0xEF > ${d}/bDeviceClass
    echo 0x02 > ${d}/bDeviceSubClass
    echo 0x01 > ${d}/bDeviceProtocol

    mkdir -p ${d}/strings/0x409
    tr -d '\0' < /proc/device-tree/serial-number > ${d}/strings/0x409/serialnumber
    echo "STMicroelectronics" > ${d}/strings/0x409/manufacturer
    echo "STM32MP25" > ${d}/strings/0x409/product

    # Config
    mkdir -p ${d}/configs/${c}
    mkdir -p ${d}/configs/${c}/strings/0x409
    echo "CDC ACM" > ${d}/configs/${c}/strings/0x409/configuration

    # Create ACM function
    mkdir -p ${d}/functions/${func_acm}
    ln -s ${d}/functions/${func_acm} ${d}/configs/${c}

    # Bind USB Device Controller
    echo ${udc} > ${d}/UDC
}

do_stop() {
    func_acm_dir=${d}/functions/${func_acm}
    if [ ! -d "${func_acm_dir}" ];
    then
        echo "Nothing to do"
        return
    fi

    echo "" > ${d}/UDC

    # Delete ACM gadget
    [ -L ${d}/configs/${c}/${func_acm} ] && rm ${d}/configs/${c}/${func_acm}
    [ -d ${d}/functions/${func_acm} ] && rmdir ${d}/functions/${func_acm}

    [ -d ${d}/configs/${c}/strings/0x409 ] && rmdir ${d}/configs/${c}/strings/0x409
    [ -d ${d}/configs/${c} ] && rmdir ${d}/configs/${c}
    [ -d ${d}/strings/0x409/ ] && rmdir ${d}/strings/0x409/
    [ -d ${d} ] && rmdir ${d}
}

case $1 in
    start)
        echo "Start usb acm gadget"
        do_start
        ;;
    stop)
        echo "Stop usb acm gadget"
        do_stop
        ;;
    restart)
        echo "Stop usb acm gadget"
        do_stop
        sleep 1
        echo "Start usb acm gadget"
        do_start
        ;;
    *)
        echo "Usage: $0 (stop | start | restart)"
        ;;
esac
