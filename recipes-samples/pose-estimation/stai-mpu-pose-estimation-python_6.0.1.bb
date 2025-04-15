# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu Python Computer Vision pose estimation application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://stai-mpu/LICENSE;md5=4e6d91bf6c3941ff01a5c6e029e2a756"

NO_GENERIC_LICENSE[SLA0044] = "stai-mpu/LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI  =  " file://stai-mpu;subdir=${BPN}-${PV} "

# Only compatible with stm32mp2
COMPATIBLE_MACHINE = "stm32mp2common"

S = "${WORKDIR}/${BPN}-${PV}"

inherit python3-dir

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/pose-estimation/

    # install license
    install -m 0444 ${S}/stai-mpu/LICENSE ${D}${prefix}/local/x-linux-ai/pose-estimation/

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/400-stai-mpu-pose-estimation-py-ovx.yaml ${D}${prefix}/local/demo/gtk-application

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/*.py              ${D}${prefix}/local/x-linux-ai/pose-estimation/
    install -m 0755 ${S}/stai-mpu/launch_python*.sh ${D}${prefix}/local/x-linux-ai/pose-estimation/
}

PACKAGES += " ${PN}-ovx-npu "
PROVIDES += " ${PN}-ovx-npu "

FILES:${PN} += " ${prefix}/local/x-linux-ai/pose-estimation/ "
FILES:${PN}-ovx-npu += " ${prefix}/local/demo/gtk-application/400-stai-mpu-pose-estimation-py-ovx.yaml "

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

RDEPENDS:${PN} += " pose-estimation-models-yolov8n "
RDEPENDS:${PN}-ovx-npu += " ${PN} stai-mpu-ovx config-npu "
