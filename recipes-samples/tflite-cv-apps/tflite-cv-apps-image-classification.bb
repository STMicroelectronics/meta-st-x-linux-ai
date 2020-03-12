# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Computer Vision image classification application examples"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

inherit pkgconfig

DEPENDS += "tensorflow-lite-staticdev gtk+3 opencv gstreamer1.0"

SRC_URI  = " file://100-tflite-image-classification-python.yaml;subdir=${PN}-${PV} "
SRC_URI += " file://101-tflite-image-classification-C++.yaml;subdir=${PN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_C++.png;subdir=${PN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_Python.png;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/label_tfl_gst_gtk.cc;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/wrapper_tfl.cc;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/wrapper_tfl.h;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/Makefile_label;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/launch_bin_label_tfl_mobilenet.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/bin/launch_bin_label_tfl_mobilenet_testdata.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/python/label_tfl_multiprocessing.py;subdir=${PN}-${PV} "
SRC_URI += " file://applications/python/launch_python_label_tfl_mobilenet.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/python/launch_python_label_tfl_mobilenet_testdata.sh;subdir=${PN}-${PV} "
SRC_URI += " file://applications/resources;subdir=${PN}-${PV} "

SRC_URI += " https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip;subdir=${PN}-${PV}/mobilenet_v1_1.0_224_quant;name=mobilenet_v1_1.0_224_quant "
SRC_URI[mobilenet_v1_1.0_224_quant.md5sum] = "38ac0c626947875bd311ef96c8baab62"
SRC_URI[mobilenet_v1_1.0_224_quant.sha256sum] = "2f8054076cf655e1a73778a49bd8fd0306d32b290b7e576dda9574f00f186c0f"

SRC_URI += " http://download.tensorflow.org/models/mobilenet_v1_2018_02_22/mobilenet_v1_0.5_128_quant.tgz;subdir=${PN}-${PV}/mobilenet_v1_0.5_128_quant;name=mobilenet_v1_0.5_128_quant "
SRC_URI[mobilenet_v1_0.5_128_quant.md5sum] = "170a3b882e57a5e5e04d913333ff21f7"
SRC_URI[mobilenet_v1_0.5_128_quant.sha256sum] = "3b5c9a8fccc4fd2f7c2a8fd2852b498294e76f81b3d19c007951fb04877db00c"

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S}/applications/bin -f Makefile_label
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/computer-vision
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification/resources
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification/models
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification/models/mobilenet
    install -d ${D}${prefix}/local/demo-ai/computer-vision/image-classification/models/mobilenet/testdata

    # install applications into the demo launcher
    install -m 0755 ${S}/*.yaml							${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*						${D}${prefix}/local/demo-ai/computer-vision/image-classification/resources

    # install python scripts and launcher scripts
    install -m 0755 ${S}/applications/python/*					${D}${prefix}/local/demo-ai/computer-vision/image-classification/python

    # install application binaries and launcher scripts
    install -m 0755 ${S}/applications/bin/*_gtk					${D}${prefix}/local/demo-ai/computer-vision/image-classification/bin
    install -m 0755 ${S}/applications/bin/*.sh					${D}${prefix}/local/demo-ai/computer-vision/image-classification/bin

    # install the resources
    install -m 0755 ${S}/applications/resources/*				${D}${prefix}/local/demo-ai/computer-vision/image-classification/resources

    # install mobilenet models
    install -m 0644 ${S}/mobilenet_v1_1.0_224_quant/label*.txt			${D}${prefix}/local/demo-ai/computer-vision/image-classification/models/mobilenet/labels.txt
    install -m 0644 ${S}/mobilenet_v1_1.0_224_quant/*.tflite			${D}${prefix}/local/demo-ai/computer-vision/image-classification/models/mobilenet/
    install -m 0644 ${S}/mobilenet_v1_0.5_128_quant/*.tflite			${D}${prefix}/local/demo-ai/computer-vision/image-classification/models/mobilenet/
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
"
