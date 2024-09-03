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

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/105-stai-mpu-image-classification-py-tfl.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/106-stai-mpu-image-classification-py-ort.yaml   ${D}${prefix}/local/demo/gtk-application

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/*.py              ${D}${prefix}/local/x-linux-ai/image-classification/
    install -m 0755 ${S}/stai-mpu/launch_python*.sh ${D}${prefix}/local/x-linux-ai/image-classification/
}

do_install:append:stm32mp25common(){
    install -m 0755 ${S}/stai-mpu/107-stai-mpu-image-classification-py-ovx.yaml ${D}${prefix}/local/demo/gtk-application/
}

PACKAGES += " ${PN}-tfl ${PN}-ort ${PN}-ovx "
PROVIDES += " ${PN}-tfl ${PN}-ort ${PN}-ovx "

FILES:${PN} += "${prefix}/local/x-linux-ai/image-classification/ "
FILES:${PN}-tfl += "${prefix}/local/demo/gtk-application/105-stai-mpu-image-classification-py-tfl.yaml "
FILES:${PN}-ort += "${prefix}/local/demo/gtk-application/106-stai-mpu-image-classification-py-ort.yaml "
FILES:${PN}-ovx:append:stm32mp25common = "${prefix}/local/demo/gtk-application/107-stai-mpu-image-classification-py-ovx.yaml "

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
