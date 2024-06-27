# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu C++ API Computer Vision image classification application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://stai-mpu/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "stai-mpu/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig

DEPENDS += " stai-mpu gtk+3 opencv gstreamer1.0 rapidjson gstreamer1.0-plugins-base gstreamer1.0-plugins-bad"

SRC_URI  = " file://stai-mpu;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'
EXTRA_OEMAKE += 'ARCHITECTURE="${BOARD_USED}"'

do_compile() {
    #Check the version of OpenCV and fill OPENCV_VERSION accordingly
    FILE=${RECIPE_SYSROOT}/${libdir}/pkgconfig/opencv4.pc
    if [ -f "$FILE" ]; then
        OPENCV_VERSION=opencv4
    else
        OPENCV_VERSION=opencv
    fi

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} -C ${S}/stai-mpu/
}

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/image-classification/

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/101-stai-mpu-image-classification-cpp-tfl.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/102-stai-mpu-image-classification-cpp-ort.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/104-stai-mpu-image-classification-cpp-coral.yaml ${D}${prefix}/local/demo/gtk-application

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/stai_mpu_image_classification ${D}${prefix}/local/x-linux-ai/image-classification/
    install -m 0755 ${S}/stai-mpu/launch_bin*.sh                ${D}${prefix}/local/x-linux-ai/image-classification/
}

do_install:append:stm32mp25common(){
    install -m 0755 ${S}/stai-mpu/103-stai-mpu-image-classification-cpp-ovx.yaml ${D}${prefix}/local/demo/gtk-application/
}

PACKAGES += " ${PN}-tfl ${PN}-ort ${PN}-ovx ${PN}-coral"
PROVIDES += " ${PN}-tfl ${PN}-ort ${PN}-ovx ${PN}-coral"

FILES:${PN} += "${prefix}/local/x-linux-ai/image-classification/ "
FILES:${PN}-tfl += "${prefix}/local/demo/gtk-application/101-stai-mpu-image-classification-cpp-tfl.yaml "
FILES:${PN}-ort += "${prefix}/local/demo/gtk-application/102-stai-mpu-image-classification-cpp-ort.yaml "
FILES:${PN}-coral += "${prefix}/local/demo/gtk-application/104-stai-mpu-image-classification-cpp-coral.yaml "
FILES:${PN}-ovx:append:stm32mp25common = "${prefix}/local/demo/gtk-application/103-stai-mpu-image-classification-cpp-ovx.yaml "

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
    stai-mpu \
    rapidjson \
    application-resources \
    bash \
"

RDEPENDS:${PN}:append:stm32mp25common = " img-models-mobilenetv2-10-224 "
RDEPENDS:${PN}:append:stm32mp1common  = " img-models-mobilenetv1-05-128 "

RDEPENDS:${PN}-tfl += " ${PN} stai-mpu-tflite "
RDEPENDS:${PN}-ort += " ${PN} stai-mpu-ort "
RDEPENDS:${PN}-coral += " ${PN} stai-mpu-tflite "
RDEPENDS:${PN}-ovx:append:stm32mp25common = " ${PN} stai-mpu-ovx "
