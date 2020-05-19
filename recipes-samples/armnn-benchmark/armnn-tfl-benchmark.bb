# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "armNN TfLite benchmark application"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"

DEPENDS += "armnn"

SRC_URI  = " file://armnn_tfl_benchmark.cc;subdir=${PN}-${PV} "
SRC_URI += " file://Makefile;subdir=${PN}-${PV} "
SRC_URI += " file://wrapper_armnn_tfl.hpp;subdir=${PN}-${PV} "

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"

do_compile() {
    oe_runmake -C ${S} -f Makefile
}

do_install() {
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/benchmark
    install -d ${D}${prefix}/local/demo-ai/benchmark/armnn-tfl

    # install application binaries and launcher scripts
    install -m 0755 ${S}/armnn_tfl_benchmark	${D}${prefix}/local/demo-ai/benchmark/armnn-tfl
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
	armnn \
"
