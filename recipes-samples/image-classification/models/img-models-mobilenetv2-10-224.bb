# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing MobileNetV2 models"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

PV="6.0.1"

#Mobilenet v2 models in TFLite, ONNX and NBG
SRC_URI = "	file://LICENSE \
			file://mobilenet_v2_1.0_224_int8_per_tensor.tflite   \
			file://mobilenet_v2_1.0_224_int8_per_tensor.onnx 	 \
			file://mobilenet_v2_1.0_224_int8_per_tensor.nb		 \
			file://labels_imagenet_2012.txt "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet
	install -d ${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/testdata

	# install mobilenet models
	install -m 0644 ${S}/labels_imagenet_2012.txt 						${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/
	install -m 0644 ${S}/mobilenet_v2_1.0_224_int8_per_tensor*			${D}${prefix}/local/x-linux-ai/image-classification/models/mobilenet/
}

FILES:${PN} += "${prefix}/local/"
