# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "armNN TfLite Computer Vision object detection example"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"

inherit pkgconfig

DEPENDS += "armnn gtk+3 opencv gstreamer1.0"

SRC_URI  = " file://object-detection/src;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/armNN_tflite_C++.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80_next_inference.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/close_50x50_pink.png;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

do_compile() {
    oe_runmake -C ${S}/object-detection/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-object-detection/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-object-detection/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/src/*.yaml	${D}${prefix}/local/demo/application

    # install the icons
    install -m 0755 ${S}/resources/*			${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-object-detection/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/object-detection/src/*_gtk	${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-object-detection/bin
    install -m 0755 ${S}/object-detection/src/*.sh	${D}${prefix}/local/demo-ai/computer-vision/armnn-tfl-object-detection/bin
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
	tflite-models-coco-ssd-mobilenetv1 \
"
