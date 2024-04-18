# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite C++ API Computer Vision object detection application example running on the EdgeTPU"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://object-detection/src/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "object-detection/src/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig

DEPENDS += "tensorflow-lite libedgetpu gtk+3 opencv gstreamer1.0 rapidjson gstreamer1.0-plugins-base gstreamer1.0-plugins-bad "

SRC_URI  = " file://object-detection/src/LICENSE;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/211-tflite-object-detection-C++-edgetpu.yaml;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/objdetect_tfl_gst_gtk.cc;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/launch_bin_objdetect_tfl_edgetpu_coco_ssd_mobilenet.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/launch_bin_objdetect_tfl_edgetpu_coco_ssd_mobilenet_testdata.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/Default.css;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/Makefile;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/wrapper_tfl.hpp;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_EdgeTPU_C++.png;subdir=${BPN}-${PV} "
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

EXTRA_OEMAKE   = 'SYSROOT="${RECIPE_SYSROOT}"'
EXTRA_OEMAKE  += 'EDGETPU=TRUE'

do_compile() {
    #Check the version of OpenCV and fill OPENCV_VERSION accordingly
    FILE=${RECIPE_SYSROOT}/${libdir}/pkgconfig/opencv4.pc
    if [ -f "$FILE" ]; then
        OPENCV_VERSION=opencv4
    else
        OPENCV_VERSION=opencv
    fi

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} -C ${S}/object-detection/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/src/*.yaml ${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*                 ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/object-detection/src/*_gtk  ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin
    install -m 0755 ${S}/object-detection/src/*.sh   ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin
    install -m 0755 ${S}/object-detection/src/*.css  ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/resources

}

FILES:${PN} += "${prefix}/local/"

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += " \
	gstreamer1.0-plugins-bad-waylandsink \
	gstreamer1.0-plugins-bad-debugutilsbad \
	gstreamer1.0-plugins-base-app \
	gstreamer1.0-plugins-base-videorate \
	gstreamer1.0-plugins-good-video4linux2 \
	gstreamer1.0-plugins-base-videoconvertscale \
	gtk+3 \
	libedgetpu \
	libopencv-core \
	libopencv-imgproc \
	libopencv-imgcodecs \
	tflite-models-coco-ssd-mobilenetv1-edgetpu \
	bash \
"
