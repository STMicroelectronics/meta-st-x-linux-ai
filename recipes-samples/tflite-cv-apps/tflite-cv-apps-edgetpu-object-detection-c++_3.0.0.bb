# Copyright (C) 2020, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite C++ API Computer Vision object detection application example running on the EdgeTPU"
LICENSE = "BSD-3-Clause & Apache-2.0"
LIC_FILES_CHKSUM  = "file://${COMMON_LICENSE_DIR}/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LIC_FILES_CHKSUM += "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit pkgconfig

DEPENDS += "tensorflow-lite libedgetpu gtk+3 opencv gstreamer1.0 rapidjson gstreamer1.0-plugins-base gstreamer1.0-plugins-bad "

SRC_URI  = " file://object-detection/src/211-tflite-object-detection-C++-edgetpu.yaml;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/objdetect_tfl_gst_gtk.cc;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/launch_bin_objdetect_tfl_edgetpu_coco_ssd_mobilenet.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/launch_bin_objdetect_tfl_edgetpu_coco_ssd_mobilenet_testdata.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/Default.css;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/Makefile;subdir=${BPN}-${PV} "
SRC_URI += " file://object-detection/src/wrapper_tfl.hpp;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/TensorFlowLite_EdgeTPU_C++.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_42x52.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_130x160.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_next_inference_42x52.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_next_inference_65x80.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/st_icon_tpu_next_inference_130x160.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_25x25.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/exit_50x50.png;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/setup_camera.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/config_board.sh;subdir=${BPN}-${PV} "
SRC_URI += " file://resources/check_camera_preview.sh;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE   = 'SYSROOT="${RECIPE_SYSROOT}"'
EXTRA_OEMAKE  += 'EDGETPU=TRUE'

do_compile() {
    #Check the version of OpenCV and fill OPENCV_VERSION accordingly
    FILE=${RECIPE_SYSROOT}/${libdir}/pkgconfig/opencv4.pc
    if [ -f "$FILE" ]; then
        OPENCV_VERSION=opencv4
    else
        OPENCV_VERSION=opencv
    fi

    #Check the gstreamer-wayland version and change API accordingly
    NEW_GST_WAYLAND_API=0
    NEW_GST_WAYLAND_API_VERSION="1.22.0"
    GST_WAYLAND_PC_FILE=${RECIPE_SYSROOT}/${libdir}/pkgconfig/gstreamer-wayland-1.0.pc
    if [ -f "$GST_WAYLAND_PC_FILE" ]; then
        GST_WAYLAND_VERSION=$(grep 'Version:' $GST_WAYLAND_PC_FILE | sed 's/^.*: //')
        if [ "$(printf '%s\n' "$GST_WAYLAND_VERSION" "$NEW_GST_WAYLAND_API_VERSION" | sort -V | head -n1)" = "$NEW_GST_WAYLAND_API_VERSION" ]; then
            NEW_GST_WAYLAND_API=1
        fi
    fi

    oe_runmake OPENCV_PKGCONFIG=${OPENCV_VERSION} NEW_GST_WAYLAND_API=${NEW_GST_WAYLAND_API} -C ${S}/object-detection/src
}

do_install() {
    install -d ${D}${prefix}/local/demo/
    install -d ${D}${prefix}/local/demo/application
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin
    install -d ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/resources

    # install applications into the demo launcher
    install -m 0755 ${S}/object-detection/src/*.yaml ${D}${prefix}/local/demo/application
    # install the icons for the demo launcher
    install -m 0755 ${S}/resources/*                 ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/resources

    # install application binaries and launcher scripts
    install -m 0755 ${S}/object-detection/src/*_gtk  ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin
    install -m 0755 ${S}/object-detection/src/*.sh   ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin
    install -m 0755 ${S}/object-detection/src/*.css  ${D}${prefix}/local/demo-ai/computer-vision/tflite-object-detection-edgetpu/bin/resources

}

FILES:${PN} += "${prefix}/local/"

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += " \
	gstreamer1.0-plugins-bad-waylandsink \
	gstreamer1.0-plugins-bad-debugutilsbad \
	gstreamer1.0-plugins-base-app \
	gstreamer1.0-plugins-base-videorate \
	gstreamer1.0-plugins-good-video4linux2 \
	gtk+3 \
	libedgetpu \
	libopencv-core \
	libopencv-imgproc \
	libopencv-imgcodecs \
	tflite-models-coco-ssd-mobilenetv1-edgetpu \
	bash \
"

#Depending of the Gstreamer version supported by the Yocto version the RDEPENDS differs
RDEPENDS:${PN} += "${@bb.utils.contains('DISTRO_CODENAME', 'kirkstone', ' gstreamer1.0-plugins-base-videoscale gstreamer1.0-plugins-base-videoconvert ', ' gstreamer1.0-plugins-base-videoconvertscale ',  d)}"
