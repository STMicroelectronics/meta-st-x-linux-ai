# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "TensorFlowLite Computer Vision application examples"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

inherit pkgconfig

DEPENDS += "tensorflow-lite-staticdev gtk+3 opencv gstreamer1.0"

SRC_URI  = " file://bin;subdir=${PN}-${PV} "
SRC_URI += " file://python;subdir=${PN}-${PV} "
SRC_URI += " file://resources;subdir=${PN}-${PV} "

SRC_URI += " https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip;subdir=${PN}-${PV}/mobilenet_v1_1.0_224_quant;name=mobilenet_v1_1.0_224_quant "
SRC_URI[mobilenet_v1_1.0_224_quant.md5sum] = "38ac0c626947875bd311ef96c8baab62"
SRC_URI[mobilenet_v1_1.0_224_quant.sha256sum] = "2f8054076cf655e1a73778a49bd8fd0306d32b290b7e576dda9574f00f186c0f"

SRC_URI += " http://download.tensorflow.org/models/mobilenet_v1_2018_02_22/mobilenet_v1_0.5_128_quant.tgz;subdir=${PN}-${PV}/mobilenet_v1_0.5_128_quant;name=mobilenet_v1_0.5_128_quant "
SRC_URI[mobilenet_v1_0.5_128_quant.md5sum] = "170a3b882e57a5e5e04d913333ff21f7"
SRC_URI[mobilenet_v1_0.5_128_quant.sha256sum] = "3b5c9a8fccc4fd2f7c2a8fd2852b498294e76f81b3d19c007951fb04877db00c"

SRC_URI += " http://storage.googleapis.com/download.tensorflow.org/models/tflite/coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.zip;subdir=${PN}-${PV}/coco_ssd_mobilenet_v1_1.0_quant;name=coco_ssd_mobilenet_v1_1.0_quant "
SRC_URI[coco_ssd_mobilenet_v1_1.0_quant.md5sum] = "3764f289165250252d2323d718c04d83"
SRC_URI[coco_ssd_mobilenet_v1_1.0_quant.sha256sum] = "a809cd290b4d6a2e8a9d5dad076e0bd695b8091974e0eed1052b480b2f21b6dc"

S = "${WORKDIR}/${PN}-${PV}"

do_configure[noexec] = "1"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile() {
    oe_runmake -C ${S}/bin
}

do_install() {
    install -d ${D}${prefix}/local/demo-ai/
    install -d ${D}${prefix}/local/demo-ai/ai-cv
    install -d ${D}${prefix}/local/demo-ai/ai-cv/bin
    install -d ${D}${prefix}/local/demo-ai/ai-cv/python
    install -d ${D}${prefix}/local/demo-ai/ai-cv/resources
    install -d ${D}${prefix}/local/demo-ai/ai-cv/models
    install -d ${D}${prefix}/local/demo-ai/ai-cv/models/mobilenet
    install -d ${D}${prefix}/local/demo-ai/ai-cv/models/mobilenet/testdata
    install -d ${D}${prefix}/local/demo-ai/ai-cv/models/coco_ssd_mobilenet/
    install -d ${D}${prefix}/local/demo-ai/ai-cv/models/coco_ssd_mobilenet/testdata

    # install python scripts and launcher scripts
    install -m 0755 ${S}/python/*					${D}${prefix}/local/demo-ai/ai-cv/python

    # install application binaries and launcher scripts
    install -m 0755 ${S}/bin/*_gtk					${D}${prefix}/local/demo-ai/ai-cv/bin
    install -m 0755 ${S}/bin/*.sh					${D}${prefix}/local/demo-ai/ai-cv/bin

    # install the resources
    install -m 0755 ${S}/resources/*					${D}${prefix}/local/demo-ai/ai-cv/resources

    # install mobilenet models
    install -m 0644 ${S}/mobilenet_v1_1.0_224_quant/label*.txt		${D}${prefix}/local/demo-ai/ai-cv/models/mobilenet/labels.txt
    install -m 0644 ${S}/mobilenet_v1_1.0_224_quant/*.tflite		${D}${prefix}/local/demo-ai/ai-cv/models/mobilenet/
    install -m 0644 ${S}/mobilenet_v1_0.5_128_quant/*.tflite		${D}${prefix}/local/demo-ai/ai-cv/models/mobilenet/

    # install coco ssd mobilenet model
    # label file of the coco ssd mobilenet may be wrong, patch it before installation
    if [ "$(sed -n '/^???/p;q'  ${S}/coco_ssd_mobilenet_v1_1.0_quant/label*.txt)" = "???" ]; then
        # if the first line match '???' string, remove it
        sed -i '1d' ${S}/coco_ssd_mobilenet_v1_1.0_quant/label*.txt
    fi;
    install -m 0644 ${S}/coco_ssd_mobilenet_v1_1.0_quant/label*.txt	${D}${prefix}/local/demo-ai/ai-cv/models/coco_ssd_mobilenet/labels.txt
    install -m 0644 ${S}/coco_ssd_mobilenet_v1_1.0_quant/*.tflite	${D}${prefix}/local/demo-ai/ai-cv/models/coco_ssd_mobilenet/
}

PACKAGES_remove = "${PN}-dev"
RDEPENDS_${PN}-staticdev = ""

FILES_${PN} += "${prefix}/local/demo-ai/ai-cv"
INSANE_SKIP_${PN} = "ldflags"

RDEPENDS_${PN} += " \
	gstreamer1.0 \
	gtk+3 \
	python3 \
	python3-pygobject \
	python3-tensorflow-lite \
	python3-opencv \
	python3-numpy \
	python3-pillow \
	python3-multiprocessing \
	python3-threading \
	python3-ctypes \
	opencv \
"
