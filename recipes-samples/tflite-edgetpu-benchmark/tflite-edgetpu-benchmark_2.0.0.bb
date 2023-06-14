# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TfLite-edgetpu benchmark application"
LICENSE = "BSD-3-Clause & Apache-2.0"
LIC_FILES_CHKSUM  = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LIC_FILES_CHKSUM += "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS += "tensorflow-lite libedgetpu"

SRC_URI  = " file://tflite_edgetpu_benchmark.cc;subdir=${BPN}-${PV} "
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
