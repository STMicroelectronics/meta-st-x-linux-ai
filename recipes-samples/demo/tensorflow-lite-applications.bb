# Copyright (C) 2019, STMicroelectronics - All Rights Reserved

SUMMARY = "TensorFlowLite application examples"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

DEPENDS += "tensorflow-lite"

SRC_URI  = " file://python;subdir=${PN}-${PV} "

SRC_URI += " https://storage.googleapis.com/download.tensorflow.org/models/mobilenet_v1_1.0_224_frozen.tgz;subdir=${PN}-${PV};name=mobilenet_label "
SRC_URI[mobilenet_label.md5sum] = "a86e005c515eae1e69aabaaccdefc94a"
SRC_URI[mobilenet_label.sha256sum] = "366a2d53008df0d2a82b375e2020bbc57e43bbe19971370e47b7f74ea0aaab91"

SRC_URI += " http://download.tensorflow.org/models/mobilenet_v1_2018_02_22/mobilenet_v1_0.5_128_quant.tgz;subdir=${PN}-${PV}/mobilenet_v1_0.5_128_quant;name=mobilenet_model "
SRC_URI[mobilenet_model.md5sum] = "170a3b882e57a5e5e04d913333ff21f7"
SRC_URI[mobilenet_model.sha256sum] = "3b5c9a8fccc4fd2f7c2a8fd2852b498294e76f81b3d19c007951fb04877db00c"

SRC_URI += " https://raw.githubusercontent.com/tensorflow/tensorflow/master/tensorflow/lite/examples/label_image/testdata/grace_hopper.bmp;subdir=${PN}-${PV}/mobilenet_testdata;name=mobilenet_testdata "
SRC_URI[mobilenet_testdata.md5sum] = "195a2ba394f1d75dd0ce10607ccb8fb8"
SRC_URI[mobilenet_testdata.sha256sum] = "8c1165a143b3ac5c37fba13a918101133d549d3419c7fc474ae70a2f29263b80"

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/demo-ai/
    install -d ${D}${prefix}/local/demo-ai/python
    install -d ${D}${prefix}/local/demo-ai/models
    install -d ${D}${prefix}/local/demo-ai/models/mobilenet
    install -d ${D}${prefix}/local/demo-ai/models/mobilenet/testdata

    install -m 0755 ${S}/python/*				${D}${prefix}/local/demo-ai/python
    install -m 0644 ${S}/mobilenet_v1_1.0_224/labels.txt	${D}${prefix}/local/demo-ai/models/mobilenet/
    install -m 0644 ${S}/mobilenet_v1_0.5_128_quant/*.tflite	${D}${prefix}/local/demo-ai/models/mobilenet/
    install -m 0644 ${S}/mobilenet_testdata/*			${D}${prefix}/local/demo-ai/models/mobilenet/testdata/
}

FILES_${PN} += "${prefix}/local/demo-ai/"

RDEPENDS_${PN} += "python3 python3-pygobject gtk+3"
