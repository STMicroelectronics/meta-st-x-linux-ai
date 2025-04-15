# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing SSD mobilenet v1 models trained on COCO dataset"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

PV="6.0.1"

# SSD mobilenet v1 models in TFLite, ONNX and NBG
SRC_URI = " http://storage.googleapis.com/download.tensorflow.org/models/tflite/coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.zip;subdir=${BPN}-${PV}/coco_ssd_mobilenet_v1_1.0_quant;name=coco_ssd_mobilenet_v1_1.0_quant \
            file://ssd_mobilenet_v1_10_300_int8.onnx \
			file://labels_coco_dataset.txt \
            file://LICENSE \
		   "
SRC_URI[coco_ssd_mobilenet_v1_1.0_quant.md5sum] = "3764f289165250252d2323d718c04d83"
SRC_URI[coco_ssd_mobilenet_v1_1.0_quant.sha256sum] = "a809cd290b4d6a2e8a9d5dad076e0bd695b8091974e0eed1052b480b2f21b6dc"

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet
    install -d ${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet/testdata

	# install SSD mobilenet v1 models
    install -m 0644 ${S}/labels*.txt												                            ${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet/labels_coco_dataset.txt
    install -m 0644 ${S}/${BPN}-${PV}/coco_ssd_mobilenet_v1_1.0_quant/detect.tflite		                        ${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet/ssd_mobilenet_v1_10_300.tflite
	install -m 0644 ${S}/ssd_mobilenet_v1_10_300_int8.onnx  			            			                ${D}${prefix}/local/x-linux-ai//object-detection/models/coco_ssd_mobilenet/ssd_mobilenet_v1_10_300.onnx
}

FILES:${PN} += "${prefix}/local/"