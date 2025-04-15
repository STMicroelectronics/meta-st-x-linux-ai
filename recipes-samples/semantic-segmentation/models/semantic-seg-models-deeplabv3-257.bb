# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
# Model used is based on TFLite model https://tfhub.dev/tensorflow/lite-model/deeplabv3/1/default/1
SUMMARY = "Create package containing deeplabv3 models used for the semantic segmentation application examples"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

PV="6.0.1"

SRC_URI  =  "file://LICENSE "
SRC_URI +=  "file://deeplabv3_257_int8_per_tensor.nb "
SRC_URI +=  "file://labels_pascalvoc.txt "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/semantic-segmentation/models/deeplabv3/
    install -d ${D}${prefix}/local/x-linux-ai/semantic-segmentation/models/deeplabv3/testdata

    # install deeplabv3 model
    install -m 0644 ${S}/labels*.txt                        ${D}${prefix}/local/x-linux-ai/semantic-segmentation/models/deeplabv3/labels_pascalvoc.txt
    install -m 0644 ${S}/deeplabv3_257_int8_per_tensor.nb   ${D}${prefix}/local/x-linux-ai/semantic-segmentation/models/deeplabv3/
}

FILES:${PN} += "${prefix}/local/"
