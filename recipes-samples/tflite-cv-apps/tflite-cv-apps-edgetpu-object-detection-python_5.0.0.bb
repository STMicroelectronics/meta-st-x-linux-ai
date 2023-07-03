# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Python Computer Vision object detection application examples running on the EdgeTPU"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI  = " file://object-detection/python/210-tflite-object-detection-python-edgetpu.yaml;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/python/objdetect_tfl.py;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/python/launch_python_objdetect_tfl_edgetpu_coco_ssd_mobilenet.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/python/launch_python_objdetect_tfl_edgetpu_coco_ssd_mobilenet_testdata.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/python/Default.css;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_EdgeTPU_Python.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_42x52.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_130x160.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_next_inference_42x52.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_next_inference_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_next_inference_130x160.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_25x25.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_50x50.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/setup_camera.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/config_board.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/check_camera_preview.sh;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/python/*.yaml ${D}${prefix}/local/demo/application

    # install the icons
    install -m 0755 ${S}/resources/* ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python/resources

    # install python scripts and launcher scripts
    install -m 0755 ${S}/object-detection/python/*.py ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python
    install -m 0755 ${S}/object-detection/python/*.sh ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python
    install -m 0755 ${S}/object-detection/python/*.css ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/python/resources
}

FILES:${PN} += "${prefix}/local/"

RDEPENDS:${PN} += " \
	python3-core \
	python3-numpy \
	python3-opencv \
	python3-pillow \
	python3-pygobject \
	python3-tensorflow-lite \
	libedgetpu \
	tflite-models-coco-ssd-mobilenetv1-edgetpu \
	bash \
"
