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

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp2common = "${@bb.utils.contains('MACHINE_FEATURES', 'gpu', 'stm32mp2_npu', 'stm32mp2', d)}"
BOARD_USED:stm32mp2common := "${@bb.utils.contains('DEFAULT_BUILD_AI', 'CPU', 'stm32mp2', '${BOARD_USED}', d)}"

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

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/stai_mpu_image_classification ${D}${prefix}/local/x-linux-ai/image-classification/
    install -m 0755 ${S}/stai-mpu/launch_bin*.sh                ${D}${prefix}/local/x-linux-ai/image-classification/

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*cpp-tfl.yaml   ${D}${prefix}/local/demo/gtk-application
    install -m 0755 ${S}/stai-mpu/*cpp-ort.yaml   ${D}${prefix}/local/demo/gtk-application
}

do_install:append:stm32mp2common(){
    if [ "${BOARD_USED}" = "stm32mp2_npu" ]; then
        # install applications into the demo launcher
        install -m 0755 ${S}/stai-mpu/*cpp-tfl-mp2.yaml      ${D}${prefix}/local/demo/gtk-application
        install -m 0755 ${S}/stai-mpu/*cpp-ort-mp2.yaml      ${D}${prefix}/local/demo/gtk-application
        install -m 0755 ${S}/stai-mpu/*cpp-ovx-mp2.yaml      ${D}${prefix}/local/demo/gtk-application
    fi
}

PACKAGES += " ${PN}-tfl-cpu ${PN}-ort-cpu "
PACKAGES += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', ' ${PN}-tfl-npu ${PN}-ort-npu ${PN}-ovx-npu ', '', d)} "

PROVIDES += " ${PN}-tfl-cpu ${PN}-ort-cpu "
PROVIDES += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', ' ${PN}-tfl-npu ${PN}-ort-npu ${PN}-ovx-npu ', '', d)} "

FILES:${PN} += "${prefix}/local/x-linux-ai/image-classification/ "

FILES:${PN}-tfl-cpu:append = "${prefix}/local/demo/gtk-application/*cpp-tfl.yaml "
FILES:${PN}-ort-cpu:append = "${prefix}/local/demo/gtk-application/*cpp-ort.yaml "

FILES:${PN}-tfl-npu += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${prefix}/local/demo/gtk-application/*cpp-tfl-mp2.yaml', '', d)} "
FILES:${PN}-ort-npu += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${prefix}/local/demo/gtk-application/*cpp-ort-mp2.yaml', '', d)} "
FILES:${PN}-ovx-npu += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${prefix}/local/demo/gtk-application/*cpp-ovx-mp2.yaml', '', d)} "

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

RDEPENDS:${PN}-tfl-npu += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${PN} stai-mpu-tflite img-models-mobilenetv2-10-224 config-npu', '', d)} "
RDEPENDS:${PN}-ort-npu += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${PN} stai-mpu-ort img-models-mobilenetv2-10-224 config-npu', '', d)} "
RDEPENDS:${PN}-ovx-npu += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${PN} stai-mpu-ovx img-models-mobilenetv2-10-224 config-npu', '', d)} "

RDEPENDS:${PN}-tfl-cpu:append  = " ${PN} stai-mpu-tflite img-models-mobilenetv1-05-128 config-cpu "
RDEPENDS:${PN}-ort-cpu:append  = " ${PN} stai-mpu-ort img-models-mobilenetv1-05-128 config-cpu "

RCONFLICTS:${PN}-ort-cpu = "${PN}-ort-npu"
RCONFLICTS:${PN}-tfl-cpu = "${PN}-tfl-npu"
RCONFLICTS:${PN}-ort-npu = "${PN}-ort-cpu"
RCONFLICTS:${PN}-tfl-npu = "${PN}-tfl-cpu"