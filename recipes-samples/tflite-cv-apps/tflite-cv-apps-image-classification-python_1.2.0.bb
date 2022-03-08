# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Python Computer Vision image classification application example"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI  = " file://image-classification/python;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_Python.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_42x52.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_130x160.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_next_inference_42x52.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_next_inference_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_next_inference_130x160.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_25x25.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_50x50.png;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification/python/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/image-classification/python/*.yaml	${D}${prefix}/local/demo/application

    # install the icons
    install -m 0755 ${S}/resources/*				${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification/python/resources

    # install python scripts and launcher scripts
    install -m 0755 ${S}/image-classification/python/*.py	${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification/python
    install -m 0755 ${S}/image-classification/python/*.sh	${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification/python
    install -m 0755 ${S}/image-classification/python/*.css	${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification/python/resources
}

FILES:${PN} += "${prefix}/local/"

RDEPENDS:${PN} += " \
	python3-core \
	python3-ctypes \
	python3-multiprocessing \
	python3-numpy \
	python3-opencv \
	python3-threading \
	python3-pillow \
	python3-pygobject \
	python3-tensorflow-lite \
	tflite-models-mobilenetv1 \
"
