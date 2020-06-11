# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing MobileNetV1 models"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

SRC_URI  = " https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip;subdir=${PN}-${PV}/mobilenet_v1_1.0_224_quant;name=mobilenet_v1_1.0_224_quant "
SRC_URI[mobilenet_v1_1.0_224_quant.md5sum] = "38ac0c626947875bd311ef96c8baab62"
SRC_URI[mobilenet_v1_1.0_224_quant.sha256sum] = "2f8054076cf655e1a73778a49bd8fd0306d32b290b7e576dda9574f00f186c0f"

SRC_URI += " http://download.tensorflow.org/models/mobilenet_v1_2018_02_22/mobilenet_v1_0.5_128_quant.tgz;subdir=${PN}-${PV}/mobilenet_v1_0.5_128_quant;name=mobilenet_v1_0.5_128_quant "
SRC_URI[mobilenet_v1_0.5_128_quant.md5sum] = "170a3b882e57a5e5e04d913333ff21f7"
SRC_URI[mobilenet_v1_0.5_128_quant.sha256sum] = "3b5c9a8fccc4fd2f7c2a8fd2852b498294e76f81b3d19c007951fb04877db00c"

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet
    install -d ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/testdata

    # install mobilenet models
    install -m 0644 ${S}/mobilenet_v1_1.0_224_quant/label*.txt ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/labels.txt
    install -m 0644 ${S}/mobilenet_v1_1.0_224_quant/*.tflite   ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/
    install -m 0644 ${S}/mobilenet_v1_0.5_128_quant/*.tflite   ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/
}

PACKAGES_remove = "${PN}-dbg"
PACKAGES_remove = "${PN}-dev"
PACKAGES_remove = "${PN}-staticdev"

FILES_${PN} += "${prefix}/local/"
