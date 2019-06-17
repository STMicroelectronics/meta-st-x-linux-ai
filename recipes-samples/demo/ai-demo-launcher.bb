# Copyright (C) 2019, STMicroelectronics - All Rights Reserved

SUMMARY = "Python GTK script launch several AI use-cases"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI = " \
    file://ai_demo_launcher.py \
    file://start_up_ai_demo_launcher.sh \
    file://media \
    "

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo-ai/
    install -d ${D}${prefix}/local/demo-ai/media
    install -m 0755 ${WORKDIR}/ai_demo_launcher.py ${D}${prefix}/local/demo-ai/
    install -m 0644 ${WORKDIR}/media/* ${D}${prefix}/local/demo-ai/media/

    # start at startup
    install -d ${D}${prefix}/local/weston-start-at-startup/
    install -m 0755 ${WORKDIR}/start_up_ai_demo_launcher.sh ${D}${prefix}/local/weston-start-at-startup/
}

FILES_${PN} += "${prefix}/local/demo-ai/ ${prefix}/local/weston-start-at-startup/"

RDEPENDS_${PN} += "python3 python3-pygobject gtk+3"
