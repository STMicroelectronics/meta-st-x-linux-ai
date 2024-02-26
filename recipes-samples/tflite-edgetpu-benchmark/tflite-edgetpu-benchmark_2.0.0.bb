# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TfLite-edgetpu benchmark application"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

DEPENDS += "tensorflow-lite libedgetpu"

SRC_URI  = " file://LICENSE;subdir=${BPN}-${PV} "
SRC_URI += " file://tflite_edgetpu_benchmark.cc;subdir=${BPN}-${PV} "
SRC_URI += " file://wrapper_tfl_edgetpu.hpp;subdir=${BPN}-${PV} "
SRC_URI += " file://Makefile;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S} -f Makefile
}

do_install() {
    install -d ${D}${prefix}/local/bin/coral-edgetpu-${PV}/tools

    # install application binaries and launcher scripts
    install -m 0755 ${S}/tflite_edgetpu_benchmark	${D}${prefix}/local/bin/coral-edgetpu-${PV}/tools
}

FILES:${PN} += "${prefix}/local/bin/coral-edgetpu-${PV}/tools/tflite_edgetpu_benchmark"

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += " \
	libedgetpu \
"
