# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TfLite-edgetpu benchmark application"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6" 

DEPENDS += "tensorflow-lite-edgetpu-staticdev tensorflow-lite-edgetpu libusb1"

SRC_URI  = " file://tflite_edgetpu_benchmark.cc;subdir=${PN}-${PV} "
#SRC_URI += " file://wrapper_tfl_edgetpu.cc;subdir=${PN}-${PV} "
SRC_URI += " file://wrapper_tfl_edgetpu.hpp;subdir=${PN}-${PV} "
SRC_URI += " file://Makefile;subdir=${PN}-${PV} "

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S} -f Makefile
}

do_install() {
    install -d ${D}${prefix}/local/demo-ai/benchmark/tflite-edgetpu

    # install application binaries and launcher scripts
    install -m 0755 ${S}/tflite_edgetpu_benchmark	${D}${prefix}/local/demo-ai/benchmark/tflite-edgetpu
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
	tensorflow-lite-edgetpu \
"
