# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Computer Vision image classification application examples"
LICENSE = "BSD-3-Clause & GPLv2 & Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit pkgconfig

DEPENDS += "tensorflow-lite-staticdev gtk+3 opencv gstreamer1.0"

SRC_URI  = " file://110-tflite-object-detection-python.yaml;subdir=${PN}-${PV} "
SRC_URI += " file://111-tflite-object-detection-C++.yaml;subdir=${PN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_C++.png;subdir=${PN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_Python.png;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/objdetect_tfl_gst_gtk.cc;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/wrapper_tfl.cc;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/wrapper_tfl.h;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/Makefile_objdetect;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/launch_bin_objdetect_tfl_coco_ssd_mobilenet.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/launch_bin_objdetect_tfl_coco_ssd_mobilenet_testdata.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/python/objdetect_tfl_multiprocessing.py;subdir=${PN}-${PV} "
SRC_URI += " file://applications/python/launch_python_objdetect_tfl_coco_ssd_mobilenet.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/python/launch_python_objdetect_tfl_coco_ssd_mobilenet_testdata.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/resources;subdir=${PN}-${PV} "

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S}/applications/bin -f Makefile_objdetect
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/computer-vision/
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/*.yaml							${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*						${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/resources

    # install python scripts and launcher scripts
    install -m 0755 ${S}/applications/python/*					${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python

    # install application binaries and launcher scripts
    install -m 0755 ${S}/applications/bin/*_gtk					${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/bin
    install -m 0755 ${S}/applications/bin/*.sh					${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/bin

    # install the resources
    install -m 0755 ${S}/applications/resources/*				${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/resources
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
	gstreamer1.0 \
	gtk+3 \
	python3 \
	python3-pygobject \
	python3-tensorflow-lite \
	python3-opencv \
	python3-numpy \
	python3-pillow \
	python3-multiprocessing \
	python3-threading \
	python3-ctypes \
	opencv \
	tflite-models-coco-ssd-mobilenetv1 \
"
