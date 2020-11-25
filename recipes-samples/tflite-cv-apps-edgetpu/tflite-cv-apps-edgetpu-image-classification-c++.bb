# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite C++ API Computer Vision image classification application example running on the EdgeTPU"
LICENSE = "BSD-3-Clause & Apache-2.0"
LIC_FILES_CHKSUM  = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LIC_FILES_CHKSUM += "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit pkgconfig

DEPENDS += "tensorflow-lite-edgetpu-staticdev gtk+3 opencv gstreamer1.0 tensorflow-lite-edgetpu libusb1"

SRC_URI  = " file://image-classification/src;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_EdgeTPU_C++.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80_next_inference.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/ST7079_AI_neural_pink_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/close_50x50_pink.png;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

#Check the version of OpenCV and fill EXTRA_OEMAKE accordingly
python () {
    import os.path

    if os.path.isfile(d.getVar('RECIPE_SYSROOT') + '/usr/lib/pkgconfig/opencv4.pc'):
        d.appendVar('EXTRA_OEMAKE', ' OPENCV_PKGCONFIG=opencv4')
    else:
        d.appendVar('EXTRA_OEMAKE', ' OPENCV_PKGCONFIG=opencv')
}

do_compile() {
    oe_runmake -C ${S}/image-classification/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai
    install -d ${D}${prefix}/local/demo-ai/computer-vision
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/image-classification/src/*.yaml ${D}${prefix}/local/demo/application

    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*                     ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/image-classification/src/*_gtk  ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin
    install -m 0755 ${S}/image-classification/src/*.sh   ${D}${prefix}/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin

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
	tensorflow-lite-edgetpu \
	gtk+3 \
	libopencv-core \
	libopencv-imgproc \
	libopencv-imgcodecs \
	tflite-models-mobilenetv1-edgetpu \
"
