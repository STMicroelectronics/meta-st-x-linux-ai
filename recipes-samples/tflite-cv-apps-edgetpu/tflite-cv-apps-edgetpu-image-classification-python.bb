# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Python Computer Vision image classification application examples running on the EdgeTPU"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

inherit pkgconfig

DEPENDS += "tensorflow-lite-edgetpu"

SRC_URI  = " file://image-classification/python;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_EdgeTPU_Python.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80_next_inference.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/close_50x50_pink.png;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/computer-vision
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/python/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/image-classification/python/*.yaml ${D}${prefix}/local/demo/application

    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*                        ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/python/resources

    # install python scripts
    install -m 0755 ${S}/image-classification/python/*.py   ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/python

    # install launcher scripts
    install -m 0755 ${S}/image-classification/python/*.sh   ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/python

}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

RDEPENDS_${PN} += " \
	python3-core \
	python3-ctypes \
	python3-tensorflow-lite-edgetpu \
	python3-numpy \
	python3-opencv \
	python3-multiprocessing \
	python3-threading \
	python3-pillow \
	python3-pygobject \
	tflite-models-mobilenetv1-edgetpu \
"
