# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Python Computer Vision object detection application example"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI  = " file://object-detection/python;subdir=${PN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_Python.png;subdir=${PN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80_next_inference.png;subdir=${PN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80.png;subdir=${PN}-${PV} "
SRC_URI += " file://resources/close_50x50_pink.png;subdir=${PN}-${PV} "

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/python/*.yaml	${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*			${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python/resources

    # install python scripts and launcher scripts
    install -m 0755 ${S}/object-detection/python/*.py	${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python
    install -m 0755 ${S}/object-detection/python/*.sh	${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection/python
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

RDEPENDS_${PN} += " \
	python3-core \
	python3-ctypes \
	python3-multiprocessing \
	python3-numpy \
	python3-opencv \
	python3-runpy \
	python3-threading \
	python3-pillow \
	python3-pygobject \
	python3-tensorflow-lite \
	tflite-models-coco-ssd-mobilenetv1 \
"
