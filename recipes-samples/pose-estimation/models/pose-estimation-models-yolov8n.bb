# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing YoloV8 pose estimation models used for the application examples"
LICENSE = "AGPL-3.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=eb1e647870add0502f8f010b19de32af"

PV="6.0.1"

SRCBRANCH = "main"
SRCREV = "a77932c834023a7becbf30643c55c024d2442878"
SRC_URI  =  "git://github.com/stm32-hotspot/ultralytics.git;branch=${SRCBRANCH};name=ultralitics_st;destsuffix=ultralitics_st_git/;protocol=https "
SRC_URI  += " file://LICENSE "

S = "${WORKDIR}/ultralitics_st_git"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/pose-estimation/models/yolov8n_pose
    install -d ${D}${prefix}/local/x-linux-ai/pose-estimation/models/yolov8n_pose/testdata

    install -m 0644 ${S}/examples/YOLOv8-STEdgeAI/stedgeai_models/pose_estimation/yolov8n_256_quant_pt_uf_pose_coco-st.nb    ${D}${prefix}/local/x-linux-ai/pose-estimation/models/yolov8n_pose/
}

FILES:${PN} += "${prefix}/local/"
