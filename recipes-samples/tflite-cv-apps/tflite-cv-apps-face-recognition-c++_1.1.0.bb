# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite C++ API Computer Vision face recognition application example"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM  = "file://face-recognition/src/LICENSE;md5=ca84d10417d1f3861af152a88187e494"

inherit pkgconfig

DEPENDS += "tensorflow-lite-staticdev gtk+3 opencv gstreamer1.0 rapidjson"

SRC_URI  = " file://face-recognition/src;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_C++.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80_next_inference.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/close_50x50_pink.png;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    #Check the version of OpenCV and fill OPENCV_VERSION accordingly
    FILE=${RECIPE_SYSROOT}/usr/lib/pkgconfig/opencv4.pc
    if [ -f "$FILE" ]; then
        OPENCV_VERSION=opencv4
    else
        OPENCV_VERSION=opencv
    fi

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} -C ${S}/face-recognition/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/testdata
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/face-recognition/src/*.yaml    ${D}${prefix}/local/demo/application

    # install the icons
    install -m 0755 ${S}/resources/*                    ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/face-recognition/src/*_gtk     ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/bin
    install -m 0755 ${S}/face-recognition/src/*.sh      ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/bin
    install -m 0755 ${S}/face-recognition/src/*.css     ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/"

INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
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
"
