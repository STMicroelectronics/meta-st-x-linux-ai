# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu Python API  Python Computer Vision semantic segmentation application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://stai-mpu/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "stai-mpu/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit python3-dir

SRC_URI  =  " file://stai-mpu;subdir=${BPN}-${PV} "

# Only compatible with stm32mp25
COMPATIBLE_MACHINE = "stm32mp2common"

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install:append:stm32mp2common(){
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/semantic-segmentation/

    # install license
    install -m 0444 ${S}/stai-mpu/LICENSE ${D}${prefix}/local/x-linux-ai/semantic-segmentation/

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/300-stai-mpu-semantic-segmentation-py-ovx.yaml ${D}${prefix}/local/demo/gtk-application/

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/*.py              ${D}${prefix}/local/x-linux-ai/semantic-segmentation/
    install -m 0755 ${S}/stai-mpu/launch_python*.sh ${D}${prefix}/local/x-linux-ai/semantic-segmentation/
}

PACKAGES += " ${PN}-ovx-npu "
PROVIDES += " ${PN}-ovx-npu "

FILES:${PN} += " ${prefix}/local/x-linux-ai/semantic-segmentation/ "
FILES:${PN}-ovx-npu += " ${prefix}/local/demo/gtk-application/300-stai-mpu-semantic-segmentation-py-ovx.yaml "

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

RDEPENDS:${PN} += " semantic-seg-models-deeplabv3-257 "
RDEPENDS:${PN}-ovx-npu += " ${PN} stai-mpu-ovx config-npu "
