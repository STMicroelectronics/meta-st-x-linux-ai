# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "Example of hardware acceleration using Onnx Runtime VSINPU execution provider "
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://ort-vsinpu-ep-example/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "ort-vsinpu-ep-example/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig python3-dir

SRC_URI  =  " file://ort-vsinpu-ep-example;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

COMPATIBLE_MACHINE = "stm32mp2common"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_install() {
    install -d ${D}${prefix}/local/bin/ort-vsinpu-ep-example/

    # install application binaries and launcher scripts
    install -m 0755 ${S}/ort-vsinpu-ep-example/LICENSE                      ${D}${prefix}/local/bin/ort-vsinpu-ep-example/
    install -m 0755 ${S}/ort-vsinpu-ep-example/ort-vsinpu-ep-example.py     ${D}${prefix}/local/bin/ort-vsinpu-ep-example/
}

FILES:${PN} += "${prefix}/local/bin/ort-vsinpu-ep-example/ "

RDEPENDS:${PN} += " \
    ${PYTHON_PN}-onnxruntime      \
    img-models-mobilenetv1-05-128 \
"