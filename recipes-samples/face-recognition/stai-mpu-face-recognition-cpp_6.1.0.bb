# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu C++ API Computer Vision face recognition application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://stai-mpu/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "stai-mpu/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig

DEPENDS += " stai-mpu gtk+3 opencv gstreamer1.0 rapidjson gstreamer1.0-plugins-base gstreamer1.0-plugins-bad"

SRC_URI  = " file://stai-mpu;subdir=${BPN}-${PV} "

# Only compatible with stm32mp25
COMPATIBLE_MACHINE = "stm32mp2common"

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

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} -C ${S}/stai-mpu/
}

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/face-recognition/
    install -d ${D}${prefix}/local/x-linux-ai/face-recognition/database
    install -d ${D}${prefix}/local/x-linux-ai/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/601-stai-mpu-face-recognition-cpp-ovx.yaml   ${D}${prefix}/local/demo/gtk-application

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/stai_mpu_face_recognition       ${D}${prefix}/local/x-linux-ai/face-recognition/
    install -m 0755 ${S}/stai-mpu/launch_bin*.sh                  ${D}${prefix}/local/x-linux-ai/face-recognition/
    install -m 0755 ${S}/stai-mpu/*.css                           ${D}${prefix}/local/x-linux-ai/resources
}

pkg_postinst:${PN} () {
    chmod 777 $D${prefix}/local/x-linux-ai/face-recognition/
    chmod 777 $D${prefix}/local/x-linux-ai/face-recognition/database
}

PACKAGES += " ${PN}-ovx-npu "
PROVIDES += " ${PN}-ovx-npu "

FILES:${PN} += "${prefix}/local/x-linux-ai/face-recognition/  ${prefix}/local/x-linux-ai/resources/"
FILES:${PN}-ovx-npu += "${prefix}/local/demo/gtk-application/601-stai-mpu-face-recognition-cpp-ovx.yaml "

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

RDEPENDS:${PN}-ovx-npu += " fd-models-blazeface-128 \
                            fr-models-facenet-512   \
                            ${PN} stai-mpu-ovx      \
                            config-npu              \
"
