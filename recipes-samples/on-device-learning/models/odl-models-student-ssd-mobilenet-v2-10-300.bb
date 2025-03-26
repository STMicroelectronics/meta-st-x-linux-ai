# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing SSD mobilenet v2 training artifacts with 2 classes"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://student_model/LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

# SSD mobilenet v2 training artifacts for teacher-student learning

SRC_URI = " file://student_model/training_model.onnx \
            file://student_model/optimizer_model.onnx \
            file://student_model/inference_model.onnx \
            file://student_model/eval_model.onnx \
            file://student_model/checkpoint \
            file://student_model/labels.txt \
            file://student_model/LICENSE \
            "

S = "${WORKDIR}"

inherit useradd

# Define the package that will contain the user
USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "--home /home/jupyter --shell /bin/sh --user-group -G video,input,tty,audio,dialout jupyter"

do_configure[noexec] = "1"
do_compile[noexec] = "1"


COMPATIBLE_MACHINE = "stm32mp25common"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/training_artifacts/
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/inference_artifacts/

    # install SSD mobilenet v2 training artifacts and labels file
    install -m 0644 -o jupyter -g jupyter ${S}/student_model/labels.txt               ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/
    install -m 0747 -o jupyter -g jupyter ${S}/student_model/checkpoint               ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/training_artifacts/
    install -m 0644 -o jupyter -g jupyter ${S}/student_model/eval_model.onnx          ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/training_artifacts/
    install -m 0644 -o jupyter -g jupyter ${S}/student_model/training_model.onnx      ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/training_artifacts/
    install -m 0644 -o jupyter -g jupyter ${S}/student_model/optimizer_model.onnx     ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/training_artifacts/

    # install SSD MobileNet V2 initial inference model
    install -m 0644 -o jupyter -g jupyter ${S}/student_model/inference_model.onnx     ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/inference_artifacts/

    chown jupyter:jupyter ${D}${prefix}/local/x-linux-ai/on-device-learning
    chown -R jupyter:jupyter ${D}${prefix}/local/x-linux-ai/on-device-learning/student_model
}

pkg_postinst:${PN} () {
    chmod 777 $D${prefix}/local/x-linux-ai/on-device-learning/student_model/ssd_mobilenet_v2/inference_artifacts/
}

FILES:${PN} += "${prefix}/local/x-linux-ai/on-device-learning/ "