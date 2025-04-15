# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing Blazeface models"

#Blazeface models is coming from PINTO model zoo : https://github.com/PINTO0309/PINTO_model_zoo under APACHE 2.0 Licence
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://blazeface/LICENSE;md5=2ceadd15836145e138bc85f4eb50bd15"

PV="6.0.1"
#Blazeface models in TFLite and NBG

SRC_URI = "	file://blazeface/LICENSE \
			file://blazeface/blazeface_128x128_quant.tflite   \
			file://blazeface/blazeface_128x128_quant.nb		 \
		  "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface
	install -d ${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface/testdata

	# install blazeface models
	install -m 0644 ${S}/blazeface/blazeface_128x128_quant.tflite			${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface/
}

do_install:append:stm32mp2common(){
    install -m 0644 ${S}/blazeface/blazeface_128x128_quant.nb			    			            ${D}${prefix}/local/x-linux-ai/face-recognition/models/blazeface/
}

FILES:${PN} += "${prefix}/local/"
