# Copyright (C) 2024, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing SSD mobilenet v2 fpnlite models trained on COCO dataset 80 classes"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

PV="6.0.1"

# SSD mobilenet v1 models in TFLite, ONNX and NBG
SRC_URI = " file://ssd_mobilenet_v2_fpnlite_10_256_int8.tflite \
            file://ssd_mobilenet_v2_fpnlite_10_256_int8.onnx \
            file://ssd_mobilenet_v2_fpnlite_10_256_int8_per_tensor.nb \
			file://labels_coco_dataset_80.txt \
            file://LICENSE \
		   "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet
    install -d ${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet/testdata

	# install SSD mobilenet v1 models
    install -m 0644 ${S}/labels*.txt												${D}${prefix}/local/x-linux-ai/object-detection/models/coco_ssd_mobilenet/
	install -m 0644 ${S}/ssd_mobilenet_v2_*.tflite		            			    ${D}${prefix}/local/x-linux-ai//object-detection/models/coco_ssd_mobilenet/
    install -m 0644 ${S}/ssd_mobilenet_v2_*.onnx		            			    ${D}${prefix}/local/x-linux-ai//object-detection/models/coco_ssd_mobilenet/
}

do_install:append:stm32mp2common(){
    install -m 0644 ${S}/ssd_mobilenet_v2_*.nb			    			            ${D}${prefix}/local/x-linux-ai//object-detection/models/coco_ssd_mobilenet/
}

FILES:${PN} += "${prefix}/local/"