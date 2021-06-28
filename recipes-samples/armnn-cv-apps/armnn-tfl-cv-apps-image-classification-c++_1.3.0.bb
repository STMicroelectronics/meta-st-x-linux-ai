# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "armNN TfLite Computer Vision image classification application example"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM  = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

inherit pkgconfig

DEPENDS += "armnn gtk+3 opencv gstreamer1.0"

SRC_URI  = " file://tfl-image-classification/src;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/armNN_tflite_C++.png;subdir=${BPN}-${PV} "
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

do_compile() {
    #Check the version of OpenCV and fill OPENCV_VERSION accordingly
    FILE=${RECIPE_SYSROOT}/usr/lib/pkgconfig/opencv4.pc
    if [ -f "$FILE" ]; then
        OPENCV_VERSION=opencv4
    else
        OPENCV_VERSION=opencv
    fi

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} -C ${S}/tfl-image-classification/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/tfl-image-classification/src/*.yaml	${D}${prefix}/local/demo/application

    # install the icons
    install -m 0755 ${S}/resources/*				${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/tfl-image-classification/src/*_gtk		${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin
    install -m 0755 ${S}/tfl-image-classification/src/*.sh		${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-image-classification/bin
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
	armnn \
	gstreamer1.0 \
	gstreamer1.0-plugins-bad-waylandsink \
	gstreamer1.0-plugins-bad-debugutilsbad \
	gstreamer1.0-plugins-base-app \
	gstreamer1.0-plugins-base-videoconvert \
	gstreamer1.0-plugins-base-videorate \
	gstreamer1.0-plugins-good-video4linux2 \
	gstreamer1.0-plugins-base-videoscale \
	gtk+3 \
	libopencv-core \
	libopencv-imgproc \
	libopencv-imgcodecs \
	tflite-models-mobilenetv1 \
"
