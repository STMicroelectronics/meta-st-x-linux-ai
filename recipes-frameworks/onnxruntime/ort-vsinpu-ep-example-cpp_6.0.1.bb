# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "Example of hardware acceleration using Onnx Runtime VSINPU execution provider cpp API "
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://ort-vsinpu-ep-example/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "ort-vsinpu-ep-example/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig

DEPENDS += " onnxruntime "

SRC_URI  =  " file://ort-vsinpu-ep-example;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp2common = "${@bb.utils.contains('MACHINE_FEATURES', 'gpu', 'stm32mp2_npu', 'stm32mp2', d)}"
BOARD_USED:stm32mp2common := "${@bb.utils.contains('DEFAULT_BUILD_AI', 'CPU', 'stm32mp2', '${BOARD_USED}', d)}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    if [ "${BOARD_USED}" = "stm32mp2_npu" ]; then
        oe_runmake -C ${S}/ort-vsinpu-ep-example/
    fi
}

do_install() {
    if [ "${BOARD_USED}" = "stm32mp2_npu" ]; then

        install -d ${D}${prefix}/local/bin/ort-vsinpu-ep-example/

        # install application binaries and launcher scripts
        install -m 0755 ${S}/ort-vsinpu-ep-example/ort-vsinpu-ep-example           ${D}${prefix}/local/bin/ort-vsinpu-ep-example/
    fi
}

FILES:${PN} += "${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${prefix}/local/bin/ort-vsinpu-ep-example/ort-vsinpu-ep-example', '', d)}"

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += " \
    onnxruntime               \
    onnxruntime-tools         \
    img-models-mobilenetv1-05-128 \
"