# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu C++ API Computer Vision object detection application example"
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
    install -d ${D}${prefix}/local/x-linux-ai/object-detection/

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/stai_mpu_object_detection ${D}${prefix}/local/x-linux-ai/object-detection/
    install -m 0755 ${S}/stai-mpu/launch_bin*.sh            ${D}${prefix}/local/x-linux-ai/object-detection/
}

do_install:append:stm32mp1common(){
    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*cpp-tfl.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*cpp-ort.yaml   ${D}${prefix}/local/demo/gtk-application
}

do_install:append:stm32mp25common(){
    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*cpp-tfl-mp2.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*cpp-ort-mp2.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*cpp-ovx-mp2.yaml   ${D}${prefix}/local/demo/gtk-application
}

PACKAGES += " ${PN}-tfl ${PN}-ort ${PN}-ovx "
PROVIDES += " ${PN}-tfl ${PN}-ort ${PN}-ovx "

FILES:${PN} += "${prefix}/local/x-linux-ai/object-detection/"

FILES:${PN}-tfl:append:stm32mp1common = "${prefix}/local/demo/gtk-application/*cpp-tfl.yaml "
FILES:${PN}-ort:append:stm32mp1common = "${prefix}/local/demo/gtk-application/*cpp-ort.yaml "

FILES:${PN}-tfl:append:stm32mp25common = "${prefix}/local/demo/gtk-application/*cpp-tfl-mp2.yaml "
FILES:${PN}-ort:append:stm32mp25common = "${prefix}/local/demo/gtk-application/*cpp-ort-mp2.yaml "
FILES:${PN}-ovx:append:stm32mp25common = "${prefix}/local/demo/gtk-application/*cpp-ovx-mp2.yaml "

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
    application-resources \
    bash \
"

RDEPENDS:${PN}-tfl += " ${PN} stai-mpu-tflite "
RDEPENDS:${PN}-ort += " ${PN} stai-mpu-ort "
RDEPENDS:${PN}-ovx:append:stm32mp25common = " ${PN} stai-mpu-ovx "

RDEPENDS:${PN}-tfl:append:stm32mp25common = " object-detect-models-ssd-mobilenet-v2-10-256-fpnlite "
RDEPENDS:${PN}-ort:append:stm32mp25common = " object-detect-models-ssd-mobilenet-v2-10-256-fpnlite "
RDEPENDS:${PN}-ovx:append:stm32mp25common = " object-detect-models-ssd-mobilenet-v2-10-256-fpnlite "

RDEPENDS:${PN}-tfl:append:stm32mp1common = " object-detect-models-ssd-mobilenet-v1-10-300 "
RDEPENDS:${PN}-ort:append:stm32mp1common = " object-detect-models-ssd-mobilenet-v1-10-300 "
RDEPENDS:${PN}-ovx:append:stm32mp1common = " object-detect-models-ssd-mobilenet-v1-10-300 "
