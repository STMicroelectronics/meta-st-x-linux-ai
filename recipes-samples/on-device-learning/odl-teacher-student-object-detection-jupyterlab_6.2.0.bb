# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing Teacher Student application hosted on jupyter lab"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://teacher_student/LICENSE;md5=4e6d91bf6c3941ff01a5c6e029e2a756"

NO_GENERIC_LICENSE[SLA0044] = "teacher_student/LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI = " file://teacher_student/odl_teacher_student_obj_detect.ipynb \
            file://teacher_student/odl_teacher_student_obj_detect_demo_hub.py \
            file://teacher_student/launch_jupyterlab_odl_teacher_student_obj_detect.sh \
            file://teacher_student/500-odl-teacher-student-object-detection-jupyterlab-ort.yaml \
            file://teacher_student/LICENSE \
        "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

inherit python3-dir useradd

# Define the package that will contain the user
USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "--home /home/jupyter --shell /bin/sh --user-group -G video,input,tty,audio,dialout jupyter"

COMPATIBLE_MACHINE = "stm32mp25common"

do_install() {
    install -d ${D}${prefix}/local/demo/gtk-application
    install -d -m 0757 ${D}${prefix}/local/x-linux-ai/on-device-learning/

    # install the jupyter-lab notebook file
    install -m 0644 -o jupyter -g jupyter ${S}/teacher_student/odl_teacher_student_obj_detect.ipynb      ${D}${prefix}/local/x-linux-ai/on-device-learning/

    # install the launcher script along with the demo hub
    install -m 0777 ${S}/teacher_student/*_demo_hub.py                                                   ${D}${prefix}/local/x-linux-ai/on-device-learning/
    install -m 0777 ${S}/teacher_student/launch_jupyterlab_*.sh                                          ${D}${prefix}/local/x-linux-ai/on-device-learning/

    # install applications into the demo launcher
    install -m 0755 ${S}/teacher_student/500-odl-teacher-student-object-detection-jupyterlab-ort.yaml    ${D}${prefix}/local/demo/gtk-application

    # Change the owner of the top-level directory
    chown jupyter:jupyter ${D}${prefix}/local/x-linux-ai/on-device-learning

    # Change the owner of the directory contents recursively
    chown -R jupyter:jupyter ${D}${prefix}/local/x-linux-ai/on-device-learning/*
}

pkg_postinst:${PN} () {
    chmod 777 $D${prefix}/local/x-linux-ai/on-device-learning/
}

FILES:${PN} += "${prefix}/local/x-linux-ai/on-device-learning "
FILES:${PN} += "${prefix}/local/demo/gtk-application/*-jupyterlab-ort.yaml "

RDEPENDS:${PN} += " \
    ${PYTHON_PN}-core \
    ${PYTHON_PN}-numpy \
    ${PYTHON_PN}-opencv \
    ${PYTHON_PN}-pillow \
    ${PYTHON_PN}-supervision \
    ${PYTHON_PN}-matplotlib \
    ${PYTHON_PN}-torch \
    ${PYTHON_PN}-onnx \
    ${PYTHON_PN}-onnxruntime-training \
    ${PYTHON_PN}-pygobject \
    ${PYTHON_PN}-stai-mpu \
    stai-mpu-ort \
    application-resources \
    bash \
"

RDEPENDS:${PN} += " odl-models-student-ssd-mobilenet-v2-10-300 "
RDEPENDS:${PN} += " odl-models-teacher-rtdetr-large-256 "