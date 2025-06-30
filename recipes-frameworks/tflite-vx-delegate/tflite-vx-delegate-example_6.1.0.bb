# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "Example of hardware acceleration using TFLite Vx Delegate "
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://tflite-vx-delegate-example/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "tflite-vx-delegate-example/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig python3-dir

DEPENDS += " tensorflow-lite tflite-vx-delegate"

SRC_URI  =  " file://tflite-vx-delegate-example;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

COMPATIBLE_MACHINE = "stm32mp2common"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S}/tflite-vx-delegate-example/
}

do_install() {
    install -d ${D}${prefix}/local/bin/tflite-vx-delegate-example/

    # install application binaries and launcher scripts
    install -m 0755 ${S}/tflite-vx-delegate-example/tflite-vx-delegate-example        ${D}${prefix}/local/bin/tflite-vx-delegate-example/
    install -m 0755 ${S}/tflite-vx-delegate-example/tflite-vx-delegate-example.py     ${D}${prefix}/local/bin/tflite-vx-delegate-example/
}

FILES:${PN} += "${prefix}/local/bin/tflite-vx-delegate-example/ "

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += " \
    tflite-vx-delegate \
    img-models-mobilenetv2-10-224 \
    ${PYTHON_PN}-numpy \
"