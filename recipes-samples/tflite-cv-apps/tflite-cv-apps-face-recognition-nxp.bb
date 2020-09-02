# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "NXP TensorFlowLite C++ API Computer Vision face recognition application example"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM  = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"

inherit pkgconfig

DEPENDS += "tensorflow-lite-staticdev gtk+3 opencv gstreamer1.0 virtual/egl virtual/libgles2"

SRC_URI  = " file://face-recognition/nxp;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S}/face-recognition/nxp
}

do_install() {
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition-nxp/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition-nxp/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/face-recognition/nxp/*_gtk	${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition-nxp/bin
    install -m 0755 ${S}/face-recognition/nxp/*.sh	${D}${prefix}/local/demo-ai/computer-vision/tflite-face-recognition-nxp/bin
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
	tflite-models-mobilenetv1 \
"
