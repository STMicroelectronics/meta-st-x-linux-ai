# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing YoloV8 people detection models used for the application examples"
LICENSE = "AGPL-3.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=eb1e647870add0502f8f010b19de32af"


SRCBRANCH = "main"
SRCREV = "342ac379075e7a9559e5b1164694a6ab3f201d0e"
SRC_URI  =  "git://github.com/stm32-hotspot/ultralytics.git;branch=${SRCBRANCH};name=ultralitics_st;destsuffix=ultralitics_st_git/;protocol=https "
SRC_URI  += " file://LICENSE "

S = "${WORKDIR}/ultralitics_st_git"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/people-tracking-heatmap/models/yolov8n_people

    install -m 0644 ${S}/examples/YOLOv8-STEdgeAI/stedgeai_models/object_detection/yolov8n_320_quant_pt_uf_od_coco-person-st.nb    ${D}${prefix}/local/x-linux-ai/people-tracking-heatmap/models/yolov8n_people/
}

FILES:${PN} += "${prefix}/local/"