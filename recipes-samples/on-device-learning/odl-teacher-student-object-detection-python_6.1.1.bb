# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing Teacher Student application hosted on Python GTK UI"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://teacher_student/LICENSE;md5=4e6d91bf6c3941ff01a5c6e029e2a756"

NO_GENERIC_LICENSE[SLA0044] = "teacher_student/LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI = " file://teacher_student/odl_teacher_student_obj_detect.py \
            file://teacher_student/501-odl-teacher-student-object-detection-py-ort.yaml \
            file://teacher_student/launch_python_odl_teacher_student_obj_detect.sh \
            file://teacher_student/utils \
            file://teacher_student/LICENSE \
        "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

inherit python3-dir

COMPATIBLE_MACHINE = "stm32mp25common"

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/dataset_utils/
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/student_utils/
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/teacher_utils/

    # install license
    install -m 0444 ${S}/teacher_student/LICENSE ${D}${prefix}/local/x-linux-ai/on-device-learning/

    # install applications into the demo launcher
    install -m 0755 ${S}/teacher_student/501-odl-teacher-student-object-detection-py-ort.yaml ${D}${prefix}/local/demo/gtk-application

    # install application scripts, function utils and launcher scripts
    install -m 0755 ${S}/teacher_student/*.py                     ${D}${prefix}/local/x-linux-ai/on-device-learning/
    install -m 0755 ${S}/teacher_student/launch_python*.sh        ${D}${prefix}/local/x-linux-ai/on-device-learning/
    install -m 0755 ${S}/teacher_student/utils/dataset_utils/*.py ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/dataset_utils/
    install -m 0755 ${S}/teacher_student/utils/student_utils/*.py ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/student_utils/
    install -m 0755 ${S}/teacher_student/utils/teacher_utils/*.py ${D}${prefix}/local/x-linux-ai/on-device-learning/utils/teacher_utils/
}

pkg_postinst:${PN} () {
    chmod 777 $D${prefix}/local/x-linux-ai/on-device-learning/
}

FILES:${PN} += "${prefix}/local/x-linux-ai/on-device-learning "
FILES:${PN} += "${prefix}/local/demo/gtk-application/*-py-ort.yaml "

RDEPENDS:${PN} += " \
    ${PYTHON_PN}-core \
    ${PYTHON_PN}-numpy \
    ${PYTHON_PN}-opencv \
    ${PYTHON_PN}-pillow \
    ${PYTHON_PN}-matplotlib \
    ${PYTHON_PN}-torch \
    ${PYTHON_PN}-onnx \
    ${PYTHON_PN}-onnxruntime \
    ${PYTHON_PN}-onnxruntime-training \
    ${PYTHON_PN}-pygobject \
    ${PYTHON_PN}-stai-mpu \
    stai-mpu-ort \
    application-resources \
    bash \
"

RDEPENDS:${PN} += " odl-models-student-ssd-mobilenet-v2-10-300 "
RDEPENDS:${PN} += " odl-models-teacher-rtdetr-large-256 "