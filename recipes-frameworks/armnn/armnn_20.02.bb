DESCRIPTION = "Arm NN is an inference engine for CPUs, GPUs and NPUs. It \
bridges the gap between existing NN frameworks and the underlying IP. It \
enables efficient translation of existing neural network frameworks, such as \
TensorFlow and Caffe, allowing them to run efficiently, without modification, \
across Arm Cortex-A CPUs, Arm Mali GPUs and Arm Ethos NPUs."
SUMMARY = "Arm Neural Network SDK (ArmNN)"
HOMEPAGE = "https://developer.arm.com/ip-products/processors/machine-learning/arm-nn"
LICENSE = "MIT"

LIC_FILES_CHKSUM = "file://LICENSE;md5=3e14a924c16f7d828b8335a59da64074"

SRC_URI = " https://github.com/ARM-software/armnn/archive/v${PV}.tar.gz;downloadfilename=armnn-v${PV}.tar.gz;name=armnn "
SRC_URI[armnn.md5sum] = "7183d26e8464e64e0e71f2386b4d972b"
SRC_URI[armnn.sha256sum] = "75dcaf18ede3410f7f6b7abd48924985044a7aff9a0c04302811e524cae6ba9e"

TENSORFLOW_VERSION="2.1.0"
SRC_URI += " https://github.com/tensorflow/tensorflow/archive/v${TENSORFLOW_VERSION}.tar.gz;downloadfilename=tensorflow-v${TENSORFLOW_VERSION}.tar.gz;name=tensorflow "
SRC_URI[tensorflow.md5sum] = "269414a50b46bb676a0ef9e611839528"
SRC_URI[tensorflow.sha256sum] = "638e541a4981f52c69da4a311815f1e7989bf1d67a41d204511966e1daed14f7"

S = "${WORKDIR}/armnn-${PV}"

inherit cmake

DEPENDS = " \
	chrpath-native \
	vim-native \
	boost \
	protobuf \
	flatbuffers \
	arm-compute-library \
"

EXTRA_OECMAKE=" \
	-DARMCOMPUTE_ROOT=${STAGING_DIR_TARGET}/usr/include/arm-compute-library/ \
	-DFLATBUFFERS_ROOT=${STAGING_DIR_TARGET}/usr/ \
	-DBOOST_ROOT=${STAGING_DIR_TARGET}/usr/ \
	-DFLATC_DIR=${STAGING_DIR_NATIVE}${prefix}/bin/ \
	-DTHIRD_PARTY_INCLUDE_DIRS=${STAGING_DIR_HOST}${includedir} \
	-DCMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES=${STAGING_INCDIR} \
	-DARMCOMPUTENEON=1 \
	-DBUILD_TESTS=1 \
	-DBUILD_SAMPLE_APP=1 \
	-DPROFILING=1 \
	-DBUILD_ARMNN_EXAMPLES=1 \
	-DBUILD_TF_LITE_PARSER=1 \
	-DTF_LITE_SCHEMA_INCLUDE_PATH=${WORKDIR}/tensorflow-${TENSORFLOW_VERSION}/tensorflow/lite/schema \
	-DTF_LITE_GENERATED_PATH=${WORKDIR}/tensorflow-${TENSORFLOW_VERSION}/tensorflow/lite/schema \
"

EXTRA_OECMAKE_append_arm=" \
	-DARMCOMPUTE_BUILD_DIR=${STAGING_DIR_TARGET}/usr/lib/ \
	-DFLATBUFFERS_LIBRARY=${STAGING_DIR_TARGET}/usr/lib/libflatbuffers.a \
	-DPROTOBUF_LIBRARY_DEBUG=${STAGING_DIR_TARGET}/usr/lib/libprotobuf.so.16.0.0 \
	-DPROTOBUF_LIBRARY_RELEASE=${STAGING_DIR_TARGET}/usr/lib/libprotobuf.so.16.0.0 \
"

EXTRA_OECMAKE_append_aarch64=" \
	-DARMCOMPUTE_BUILD_DIR=${STAGING_DIR_TARGET}/usr/lib64/ \
	-DFLATBUFFERS_LIBRARY=${STAGING_DIR_TARGET}/usr/lib64/libflatbuffers.a \
	-DPROTOBUF_LIBRARY_DEBUG=${STAGING_DIR_TARGET}/usr/lib64/libprotobuf.so.16.0.0 \
	-DPROTOBUF_LIBRARY_RELEASE=${STAGING_DIR_TARGET}/usr/lib64/libprotobuf.so.16.0.0 \
"

do_install() {
	install -d ${D}${libdir}
	install -d ${D}${bindir}/${PN}-${PV}/tools
	install -d ${D}${bindir}/${PN}-${PV}/tools/examples/tensorflow-lite
	install -d ${D}${includedir}
	install -d ${D}${includedir}/armnn-tensorflow-lite/schema

	# install dynamic libraries
	cp -P ${WORKDIR}/build/*.so                                                                   ${D}${libdir}
	cp -P ${WORKDIR}/build/*.so*                                                                  ${D}${libdir}

	# install static libraries
	install -m 0644 ${WORKDIR}/build/libarmnnUtils.a                                              ${D}${libdir}

	# install tools
	install -m 0555 ${WORKDIR}/build/UnitTests                                                    ${D}${bindir}/${PN}-${PV}/tools
	install -m 0555 ${WORKDIR}/build/tests/ExecuteNetwork                                         ${D}${bindir}/${PN}-${PV}/tools

	# install armnn include files
	cp -rf ${S}/include/armnn                                                                     ${D}${includedir}
	cp -rf ${S}/include/armnnDeserializer                                                         ${D}${includedir}
	cp -rf ${S}/include/armnnQuantizer                                                            ${D}${includedir}
	cp -rf ${S}/include/armnnSerializer                                                           ${D}${includedir}
	cp -rf ${S}/include/armnnUtils                                                                ${D}${includedir}

	# install armnn TfLite schema
	install -m 0644 ${WORKDIR}/tensorflow-${TENSORFLOW_VERSION}/tensorflow/lite/schema/schema.fbs ${D}${includedir}/armnn-tensorflow-lite/schema/

	# install armnn TfLite examples
	install -m 0555 ${WORKDIR}/build/tests/TfLite*                                                ${D}${bindir}/${PN}-${PV}/tools/examples/tensorflow-lite

	# install armnn TfLiteParser include files
	cp -rf ${S}/include/armnnTfLiteParser                                                         ${D}${includedir}

	# Update the rpath
	chrpath -d ${D}${bindir}/${PN}-${PV}/tools/UnitTests
	chrpath -d ${D}${bindir}/${PN}-${PV}/tools/ExecuteNetwork
	chrpath -d ${D}${bindir}/${PN}-${PV}/tools/examples/tensorflow-lite/*
	chrpath -d ${D}${libdir}/libarmnnTfLiteParser.so.*
}

PACKAGES += "${PN}-tools"
PACKAGES += "${PN}-tensorflow-lite"
PACKAGES += "${PN}-tensorflow-lite-dev"
PACKAGES += "${PN}-tensorflow-lite-examples"

RDEPENDS_${PN} += "arm-compute-library protobuf boost"
RDEPENDS_${PN}-tensorflow-lite += "${PN}"
RDEPENDS_${PN}-tensorflow-lite-dev += "${PN}-tensorflow-lite"
RDEPENDS_${PN}-tensorflow-lite-examples += "${PN}-tensorflow-lite"

FILES_${PN} = " \
	${libdir}/libarmnn.so.* \
"

FILES_${PN}-dev = " \
	${libdir}/libarmnn.so \
	${includedir}/armnn \
	${includedir}/armnnDeserializer \
	${includedir}/armnnQuantizer \
	${includedir}/armnnSerializer \
	${includedir}/armnnUtils \
"

FILES_${PN}-tools = " \
	${bindir}/${PN}-${PV}/tools/ExecuteNetwork \
	${bindir}/${PN}-${PV}/tools/UnitTests \
"

FILES_${PN}-tensorflow-lite = " \
	${libdir}/libarmnnTfLiteParser.so.* \
	${includedir}/armnn-tensorflow-lite/schema \
"

FILES_${PN}-tensorflow-lite-dev = " \
	${libdir}/libarmnnTfLiteParser.so \
	${includedir}/armnnTfLiteParser \
"

FILES_${PN}-tensorflow-lite-examples = " \
	${bindir}/${PN}-${PV}/tools/examples/tensorflow-lite/* \
"
