# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Python Computer Vision object detection application examples running on the EdgeTPU"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI  = " file://object-detection/python;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_EdgeTPU_Python.png;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/computer-vision/
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/python/*.yaml ${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*                    ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python/resources

    # install python scripts and launcher scripts
    install -m 0755 ${S}/object-detection/python/*.py   ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python
    install -m 0755 ${S}/object-detection/python/*.sh   ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python

}

FILES:${PN} += "${prefix}/local/"

RDEPENDS:${PN} += " \
	python3-core \
	python3-ctypes \
	python3-tensorflow-lite \
	python3-numpy \
	python3-opencv \
	python3-multiprocessing \
	python3-threading \
	python3-pillow \
	python3-pygobject \
	libedgetpu \
	tflite-models-coco-ssd-mobilenetv1-edgetpu \
"
