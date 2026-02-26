# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
# Released under the MIT license (see COPYING.MIT for the terms)

SUMMARY = "The goal is to enable USB gadget ACM composite configuration"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PV = "1.0"

SRC_URI = " file://stm32_usbotg_acm_config.sh \
    "

S = "${WORKDIR}/git"

do_install() {
    install -d ${D}${base_sbindir}
    install -m 0755 ${WORKDIR}/stm32_usbotg_acm_config.sh ${D}${base_sbindir}
}
