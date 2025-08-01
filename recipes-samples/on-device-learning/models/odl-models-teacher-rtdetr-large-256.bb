# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing RT-DETR ONNX models exported from Ultralytics Python"
DESCRIPTION = " recipe to install RT-DETR ONNX model as a teacher for teacher student learning "
LICENSE = "AGPL-3.0-only"
LIC_FILES_CHKSUM = "file://teacher_model/LICENSE;md5=3db23ab95801691a1b98ff9ddb8dc98b"

SRC_URI += " file://teacher_model/LICENSE "

S = "${WORKDIR}"

inherit setuptools3 useradd

DEPENDS += " \
    python3-pip-native \
    ca-certificates-native \
"

# Define the package that will contain the user
USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "--home /home/jupyter --shell /bin/sh --user-group -G video,input,tty,audio,dialout jupyter"

COMPATIBLE_MACHINE = "stm32mp25common"

do_configure[network] = "1"

do_configure() {
    if [ -n "${http_proxy}" ]; then
        export HTTP_PROXY=${http_proxy}
        export http_proxy=${http_proxy}
    fi
    if [ -n "${https_proxy}" ]; then
        export HTTPS_PROXY=${https_proxy}
        export https_proxy=${https_proxy}
    fi

    # Set the SSL certificate environment variable
    export SSL_CERT_FILE="${STAGING_DIR_NATIVE}/etc/ssl/certs/ca-certificates.crt"

    unset PYTHON
    unset FC
    pip3 install onnx ultralytics --prefix=${STAGING_DIR_NATIVE}${prefix}

    python3 <<EOF
import os
from ultralytics import RTDETR
import ssl

ssl._create_default_https_context = ssl._create_stdlib_context

# Load a model
model = RTDETR('rtdetr-l.pt')

# Export the model to ONNX format
path = model.export(format="onnx", imgsz=256)

EOF
}

do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/on-device-learning/teacher_model/rt-detr/

    # install RT-DETR onnx model and labels
    install -m 0644 -o jupyter -g jupyter ${S}/rtdetr*.onnx              		${D}${prefix}/local/x-linux-ai/on-device-learning/teacher_model/rt-detr/

    chown jupyter:jupyter ${D}${prefix}/local/x-linux-ai/on-device-learning
    chown -R jupyter:jupyter ${D}${prefix}/local/x-linux-ai/on-device-learning/teacher_model
}

FILES:${PN} += "${prefix}/local/x-linux-ai/on-device-learning "