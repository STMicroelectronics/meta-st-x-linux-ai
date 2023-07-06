# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite C++ API Computer Vision image classification application example running on the EdgeTPU"
LICENSE = "BSD-3-Clause & Apache-2.0"
LIC_FILES_CHKSUM  = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LIC_FILES_CHKSUM += "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit pkgconfig

DEPENDS += "tensorflow-lite libedgetpu gtk+3 opencv gstreamer1.0 gstreamer1.0-plugins-base gstreamer1.0-plugins-bad "

SRC_URI  = " file://image-classification/src/201-tflite-image-classification-C++-edgetpu.yaml;subdir=${BPN}-${PV} "
SRC_URI += " file://image-classification/src/label_tfl_gst_gtk.cc;subdir=${BPN}-${PV} "
SRC_URI += " file://image-classification/src/launch_bin_label_tfl_edgetpu_mobilenet.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://image-classification/src/launch_bin_label_tfl_edgetpu_mobilenet_testdata.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://image-classification/src/Default.css;subdir=${BPN}-${PV} "
SRC_URI += " file://image-classification/src/Makefile;subdir=${BPN}-${PV} "
SRC_URI += " file://image-classification/src/wrapper_tfl.hpp;subdir=${BPN}-${PV} "
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

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} -C ${S}/image-classification/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/image-classification/src/*.yaml ${D}${prefix}/local/demo/application

    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*                     ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/image-classification/src/*_gtk  ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin
    install -m 0755 ${S}/image-classification/src/*.sh   ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin
    install -m 0755 ${S}/image-classification/src/*.css      ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin/resources
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
	tflite-models-mobilenetv1-edgetpu \
	bash \
"
