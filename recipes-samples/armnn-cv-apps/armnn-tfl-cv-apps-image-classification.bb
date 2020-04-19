# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "armNN TfLite Computer Vision image classification application examples"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"

inherit pkgconfig

DEPENDS += "armnn gtk+3 opencv gstreamer1.0"

SRC_URI  = " file://120-armnn-image-classification-C++.yaml;subdir=${PN}-${PV} "
SRC_URI += " file://resources/armNN_C++.png;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/label_armnn_tfl_gst_gtk.cc;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/wrapper_armnn_tfl.hpp;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/Makefile_label;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/launch_bin_label_armnn_tfl_mobilenet.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/launch_bin_label_armnn_tfl_mobilenet_testdata.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/resources;subdir=${PN}-${PV} "

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"

do_compile() {
    oe_runmake -C ${S}/applications/bin -f Makefile_label
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/computer-vision
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/*.yaml							${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*						${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/applications/bin/*_gtk					${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin
    install -m 0755 ${S}/applications/bin/*.sh					${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin

    # install the resources
    install -m 0755 ${S}/applications/resources/*				${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/resources
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
	armnn \
	gstreamer1.0 \
	gtk+3 \
	opencv \
	tflite-models-mobilenetv1 \
"
