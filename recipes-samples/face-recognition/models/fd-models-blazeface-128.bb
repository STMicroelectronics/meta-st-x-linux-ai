# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing Blazeface models"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

#Blazeface models in TFLite and NBG

SRC_URI = "	file://LICENSE \
			file://blazeface_128x128_quant.tflite   \
			file://blazeface_128x128_quant.nb		 \
		  "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface
	install -d ${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface/testdata

	# install blazeface models
	install -m 0644 ${S}/blazeface_128x128_quant.tflite			${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface/
}

do_install:append:stm32mp25common(){
    install -m 0644 ${S}/blazeface_128x128_quant.nb			    			            ${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface/
}

FILES:${PN} += "${prefix}/local/"
