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
    install -m 0755 ${S}/stai-mpu/*.py                  ${D}${prefix}/local/x-linux-ai/image-classification/
    install -m 0755 ${S}/stai-mpu/launch_python*.sh     ${D}${prefix}/local/x-linux-ai/image-classification/

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*py-tfl.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*py-ort.yaml   ${D}${prefix}/local/demo/gtk-application
}

do_install:append:stm32mp2common(){
    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*py-tfl-mp2.yaml      ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*py-ort-mp2.yaml      ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*py-ovx-mp2.yaml      ${D}${prefix}/local/demo/gtk-application
}


PACKAGES:append:stm32mp1common = " ${PN}-tfl-cpu ${PN}-ort-cpu "
PACKAGES:append:stm32mp2common = " ${PN}-tfl-cpu ${PN}-ort-cpu ${PN}-tfl-npu ${PN}-ort-npu ${PN}-ovx-npu "

PROVIDES:append:stm32mp1common = " ${PN}-tfl-cpu ${PN}-ort-cpu "
PROVIDES:append:stm32mp2common = " ${PN}-tfl-cpu ${PN}-ort-cpu ${PN}-tfl-npu ${PN}-ort-npu ${PN}-ovx-npu "

FILES:${PN} += "${prefix}/local/x-linux-ai/image-classification/ "

FILES:${PN}-tfl-cpu:append = "${prefix}/local/demo/gtk-application/*py-tfl.yaml "
FILES:${PN}-ort-cpu:append = "${prefix}/local/demo/gtk-application/*py-ort.yaml "

FILES:${PN}-tfl-npu:append:stm32mp2common = "${prefix}/local/demo/gtk-application/*py-tfl-mp2.yaml "
FILES:${PN}-ort-npu:append:stm32mp2common = "${prefix}/local/demo/gtk-application/*py-ort-mp2.yaml "
FILES:${PN}-ovx-npu:append:stm32mp2common = "${prefix}/local/demo/gtk-application/*py-ovx-mp2.yaml "

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

RDEPENDS:${PN}-tfl-npu:append:stm32mp2common = " ${PN} stai-mpu-tflite img-models-mobilenetv2-10-224 config-npu "
RDEPENDS:${PN}-ort-npu:append:stm32mp2common = " ${PN} stai-mpu-ort img-models-mobilenetv2-10-224 config-npu "
RDEPENDS:${PN}-ovx-npu:append:stm32mp2common = " ${PN} stai-mpu-ovx img-models-mobilenetv2-10-224 config-npu "

RDEPENDS:${PN}-tfl-cpu:append  = "  ${PN} stai-mpu-tflite img-models-mobilenetv1-05-128 config-cpu "
RDEPENDS:${PN}-ort-cpu:append  = " ${PN} stai-mpu-ort img-models-mobilenetv1-05-128 config-cpu "

RCONFLICTS:${PN}-ort-cpu = "${PN}-ort-npu"
RCONFLICTS:${PN}-tfl-cpu = "${PN}-tfl-npu"
RCONFLICTS:${PN}-ort-npu = "${PN}-ort-cpu"
RCONFLICTS:${PN}-tfl-npu = "${PN}-tfl-cpu"