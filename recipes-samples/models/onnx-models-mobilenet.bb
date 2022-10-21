# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing MobileNetV1/2 models"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

# These models are not available in the onnx/models repository and integrating the tf2onnx
# converter in the yocto workflow (to convert them from tflite) would require integrating
# tensorflow (not lite) and bazel as well, which is a pretty big undertaking.
# So, for now, we will simply include the converted models manually.
SRC_URI = "file://mobilenet_v1_0.5_128.onnx \
		   file://mobilenet_v1_0.5_128_quant.onnx \
		   file://mobilenet_v1_1.0_224_quant.onnx \
		   file://labels_mobilenet_quant_v1_224_onnx.txt "

SRC_URI  += " https://github.com/onnx/models/raw/main/vision/classification/mobilenet/model/mobilenetv2-12.onnx;name=mobilenet_v2_1.0_224 "
SRC_URI[mobilenet_v2_1.0_224.md5sum] = "f52fc701d2f167f5239828cced33213e"
SRC_URI[mobilenet_v2_1.0_224.sha256sum] = "c0c3f76d93fa3fd6580652a45618618a220fced18babf65774ed169de0432ad5"

SRC_URI  += " https://github.com/onnx/models/raw/main/vision/classification/mobilenet/model/mobilenetv2-12-int8.onnx;name=mobilenet_v2_1.0_224_int8 "
SRC_URI[mobilenet_v2_1.0_224_int8.md5sum] = "52814a5921dd74c70c96204bf024adb7"
SRC_URI[mobilenet_v2_1.0_224_int8.sha256sum] = "cc028fe6cae7bc11a4ff53cfc9b79c920e8be65ce33a904ec3e2a8f66d77f95f"

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet
	install -d ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/testdata

	# install mobilenet models
	install -m 0644 ${S}/label*.txt ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/labels_onnx.txt
	install -m 0644 ${S}/*.onnx ${D}${prefix}/local/demo-ai/computer-vision/models/mobilenet/
}

FILES:${PN} += "${prefix}/local/"
