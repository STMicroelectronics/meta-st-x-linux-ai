# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "onnx C++ API Computer Vision object detection application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://object-detection/src/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "object-detection/src/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig

DEPENDS += "onnxruntime gtk+3 opencv gstreamer1.0 rapidjson gstreamer1.0-plugins-base gstreamer1.0-plugins-bad"

SRC_URI  = " file://object-detection/src/LICENSE;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/311-onnx-object-detection-C++.yaml;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/objdetect_onnx_gst_gtk.cc;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/launch_bin_objdetect_onnx_coco_ssd_mobilenet.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/launch_bin_objdetect_onnx_coco_ssd_mobilenet_testdata.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/Default.css;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/Makefile;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/wrapper_onnx.hpp;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/Onnxruntime_C++.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_42x52_onnx.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_65x80_onnx.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_130x160_onnx.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_next_inference_42x52_onnx.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_next_inference_65x80_onnx.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_next_inference_130x160_onnx.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_25x25.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_50x50.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/setup_camera.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/config_board.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/check_camera_preview.sh;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

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
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/onnx-object-detection/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/onnx-object-detection/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/src/*.yaml	${D}${prefix}/local/demo/application

    # install the icons
    install -m 0755 ${S}/resources/*		${D}${prefix}/local/demo-ai/computer-vision/onnx-object-detection/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/object-detection/src/*_gtk	${D}${prefix}/local/demo-ai/computer-vision/onnx-object-detection/bin
    install -m 0755 ${S}/object-detection/src/*.sh	${D}${prefix}/local/demo-ai/computer-vision/onnx-object-detection/bin
    install -m 0755 ${S}/object-detection/src/*.css	${D}${prefix}/local/demo-ai/computer-vision/onnx-object-detection/bin/resources

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
	libopencv-core \
	libopencv-imgproc \
	libopencv-imgcodecs \
	onnxruntime \
	onnx-models-coco-ssd-mobilenetv1 \
	rapidjson \
	bash \
"

#Depending of the Gstreamer version supported by the Yocto version the RDEPENDS differs
RDEPENDS:${PN} += "${@bb.utils.contains('DISTRO_CODENAME', 'kirkstone', ' gstreamer1.0-plugins-base-videoscale gstreamer1.0-plugins-base-videoconvert ', ' gstreamer1.0-plugins-base-videoconvertscale ',  d)}"
