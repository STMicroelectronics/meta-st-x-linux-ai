# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu Python API Computer Vision image classification application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://stai-mpu/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "stai-mpu/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig

SRC_URI  = " file://stai-mpu;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

inherit python3-dir

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/image-classification/

    # install license
    install -m 0444 ${S}/stai-mpu/LICENSE ${D}${prefix}/local/x-linux-ai/image-classification/

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/*.py              ${D}${prefix}/local/x-linux-ai/image-classification/
    install -m 0755 ${S}/stai-mpu/launch_python*.sh ${D}${prefix}/local/x-linux-ai/image-classification/
}

do_install:append:stm32mp1common(){
    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*py-tfl.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*py-ort.yaml   ${D}${prefix}/local/demo/gtk-application
}

do_install:append:stm32mp25common(){
    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*py-tfl-mp2.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*py-ort-mp2.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*py-ovx-mp2.yaml   ${D}${prefix}/local/demo/gtk-application
}


PACKAGES += " ${PN}-tfl ${PN}-ort ${PN}-ovx "
PROVIDES += " ${PN}-tfl ${PN}-ort ${PN}-ovx "

FILES:${PN} += "${prefix}/local/x-linux-ai/image-classification/ "
FILES:${PN}-tfl:append:stm32mp1common = "${prefix}/local/demo/gtk-application/*py-tfl.yaml "
FILES:${PN}-ort:append:stm32mp1common = "${prefix}/local/demo/gtk-application/*py-ort.yaml "

FILES:${PN}-tfl:append:stm32mp25common = "${prefix}/local/demo/gtk-application/*py-tfl-mp2.yaml "
FILES:${PN}-ort:append:stm32mp25common = "${prefix}/local/demo/gtk-application/*py-ort-mp2.yaml "
FILES:${PN}-ovx:append:stm32mp25common = "${prefix}/local/demo/gtk-application/*py-ovx-mp2.yaml "

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += " \
    ${PYTHON_PN}-core \
    ${PYTHON_PN}-numpy \
    ${PYTHON_PN}-opencv \
    ${PYTHON_PN}-pillow \
    ${PYTHON_PN}-pygobject \
    ${PYTHON_PN}-stai-mpu \
    application-resources \
    bash \
"

RDEPENDS:${PN}:append:stm32mp25common = " img-models-mobilenetv2-10-224 "
RDEPENDS:${PN}:append:stm32mp1common  = " img-models-mobilenetv1-05-128 "

RDEPENDS:${PN}-tfl += " ${PN} stai-mpu-tflite "
RDEPENDS:${PN}-ort += " ${PN} stai-mpu-ort "
RDEPENDS:${PN}-ovx:append:stm32mp25common = " ${PN} stai-mpu-ovx "
