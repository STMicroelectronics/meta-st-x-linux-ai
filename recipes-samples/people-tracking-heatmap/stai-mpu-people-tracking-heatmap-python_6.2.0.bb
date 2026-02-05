# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "stai_mpu python API Computer Vision people tracking heatmap application example"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://stai-mpu/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "stai-mpu/LICENSE"
LICENSE:${PN} = "SLA0044"

inherit pkgconfig
inherit python3-dir
inherit systemd

SRC_URI  = " file://stai-mpu;subdir=${BPN}-${PV} "

S = "${WORKDIR}/${BPN}-${PV}"
DEPENDS += " linux-stm32mp gstreamer1.0-plugins-bad"
# Only compatible with stm32mp2
COMPATIBLE_MACHINE = "stm32mp2common"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/people-tracking-heatmap/
    install -d ${D}${systemd_system_unitdir}

    # install license
    install -m 0444 ${S}/stai-mpu/LICENSE ${D}${prefix}/local/x-linux-ai/people-tracking-heatmap/

    # install application binaries and launcher scripts
    install -m 0755 ${S}/stai-mpu/*.py              ${D}${prefix}/local/x-linux-ai/people-tracking-heatmap/
    install -m 0755 ${S}/stai-mpu/launch_python*.sh ${D}${prefix}/local/x-linux-ai/people-tracking-heatmap/

    # Install the systemd service file

    install -m 0644 ${S}/stai-mpu/people-tracking-heatmap.service ${D}${systemd_system_unitdir}/
}

do_install:append(){
    # install applications into the demo launcher
    install -m 0755 ${S}/stai-mpu/*py-ovx.yaml   ${D}${prefix}/local/demo/gtk-application
}

PACKAGES += " ${PN}-ovx-npu "
PROVIDES += " ${PN}-ovx-npu "

FILES:${PN} += "${prefix}/local/x-linux-ai/people-tracking-heatmap/ "
FILES:${PN}-ovx-npu += "${prefix}/local/demo/gtk-application/*py-ovx.yaml "

RDEPENDS:${PN} += " \
    ${PYTHON_PN}-core \
    ${PYTHON_PN}-numpy \
    ${PYTHON_PN}-opencv \
    ${PYTHON_PN}-pillow \
    ${PYTHON_PN}-pygobject \
    ${PYTHON_PN}-stai-mpu \
    ${PYTHON_PN}-pyserial \
    application-resources \
    bash \
    usbotg-gadget-acm-config \
    usbotg-gadget-acm-uvc-config \
"

RDEPENDS:${PN} += " people-tracking-models-yolov8n \
                    gstreamer1.0-plugins-bad-uvcgadget \
                    gstreamer1.0-plugins-bad \
                    gstreamer1.0-plugins-bad-waylandsink \
                    gstreamer1.0-plugins-bad-debugutilsbad \
                  "

RDEPENDS:${PN}-ovx-npu += " ${PN} stai-mpu-ovx  python3-supervision config-npu "

SYSTEMD_SERVICE:${PN} = "people-tracking-heatmap.service"
SYSTEMD_AUTO_ENABLE:${PN} = "disable"
