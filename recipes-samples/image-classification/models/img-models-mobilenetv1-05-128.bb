# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing MobileNetV1 models"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

PV="6.0.1"

#Mobilenet v1 models in TFLite, ONNX
SRC_URI += " https://storage.googleapis.com/download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_0.5_128_quant.tgz;subdir=${BPN}-${PV}/mobilenet_v1_0.5_128_quant;name=mobilenet_v1_0.5_128_quant "
SRC_URI[mobilenet_v1_0.5_128_quant.md5sum] = "5cc8484cf04a407fc90993296f3f02db"
SRC_URI[mobilenet_v1_0.5_128_quant.sha256sum] = "0a5b18571d3df4d85a5ac6cb5be829d141dd5855243ea04422ca7d19f730a506"

SRC_URI += " file://LICENSE \
			 file://mobilenet_v1_0.5_128_quant.onnx \
			 file://labels_imagenet.txt "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet
	install -d ${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/testdata

	# install mobilenet models
	install -m 0644 ${S}/labels_imagenet.txt 									${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/
	install -m 0644 ${S}/mobilenet_v1_0.5_128_*									${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/
	install -m 0644 ${S}/${BPN}-${PV}/mobilenet_v1_0.5_128_quant/*.tflite   	${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/
}

FILES:${PN} += "${prefix}/local/"
