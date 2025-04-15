# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing Facenet512 models"

#Facenet512 model weights are coming from Deepface github : https://github.com/serengil/deepface under MIT Licence
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://facenet/LICENSE;md5=f3a6c154b2b11d824cf57057af10de88"

PV="6.0.1"
#Blazeface models in TFLite and NBG
SRC_URI = "	file://facenet/LICENSE \
			file://facenet/facenet512_160x160_quant.tflite   \
			file://facenet/facenet512_160x160_quant.nb		 \
		  "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/x-linux-ai/face-recognition/models/facenet
	install -d ${D}${prefix}/local/x-linux-ai/face-recognition/models/facenet/testdata

	# install facenet models
	install -m 0644 ${S}/facenet/facenet512_160x160_quant.tflite			${D}${prefix}/local/x-linux-ai/face-recognition/models/facenet/
}

do_install:append:stm32mp2common(){
    install -m 0644 ${S}/facenet/facenet512_160x160_quant.nb		         ${D}${prefix}/local/x-linux-ai/face-recognition/models/facenet/
}

FILES:${PN} += "${prefix}/local/"
