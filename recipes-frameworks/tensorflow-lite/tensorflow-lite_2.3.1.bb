DESCRIPTION = "TensorFlow Lite is an open source deep learning framework for \
on-device inference."
SUMMARY = "TensorFlow Lite Python interpreter and C++ Library"
HOMEPAGE = "https://www.tensorflow.org/lite"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=64a34301f8e355f57ec992c2af3e5157"

SRC_URI = " git://github.com/tensorflow/tensorflow.git;branch=r2.3 "
SRCREV = "fcc4b966f1265f466e82617020af93670141b009"
S = "${WORKDIR}/git"

inherit setuptools3

DEPENDS = " \
	coreutils-native \
	curl-native \
	gzip-native \
	unzip-native \
	swig-native \
	python3-wheel-native \
	python3-numpy-native \
	python3-pybind11-native \
	python3 \
	zlib \
"

#Recipe only tested with armv7ve
COMPATIBLE_HOST = "(arm.*).*-linux"
COMPATIBLE_HOST_armv4  = 'null'
COMPATIBLE_HOST_armv5  = 'null'
COMPATIBLE_HOST_armv6  = 'null'
COMPATIBLE_HOST_aarch64_class-target = 'null'

do_configure[noexec] = "1"
do_unpack[postfuncs] += " download_dependencies "

download_dependencies(){
	if [ -n "${http_proxy}" ]; then
		export HTTP_PROXY=${http_proxy}
		export http_proxy=${http_proxy}
	fi
	if [ -n "${https_proxy}" ]; then
		export HTTPS_PROXY=${https_proxy}
		export https_proxy=${https_proxy}
	fi
	# fix CURL certificates path
	export CURL_CA_BUNDLE="/etc/ssl/certs/ca-certificates.crt"

	${S}/tensorflow/lite/tools/make/download_dependencies.sh
}

# Set building environment variables
TENSORFLOW_TARGET="${@bb.utils.contains('TARGET_OS', 'linux-gnueabi', 'linux', '', d)}"
TENSORFLOW_TARGET_ARCH_armv7ve="${@bb.utils.contains('TUNE_FEATURES', 'cortexa7', 'armv7l', '', d)}"

do_compile () {
	export TENSORFLOW_TARGET=${TENSORFLOW_TARGET}
	export TENSORFLOW_TARGET_ARCH=${TENSORFLOW_TARGET_ARCH}
	export TENSORFLOW_CC_PREFIX=${CCACHE}${HOST_PREFIX}
	export TENSORFLOW_EXTRA_CFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"
	export TENSORFLOW_EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"

	unset PYTHON
	bash ${S}/tensorflow/lite/tools/pip_package/build_pip_package.sh
}

do_install(){
	# tensorflow-lite static lib for dev and examples
	install -d ${D}${libdir}
	install -d ${D}${includedir}/tensorflow
	install -d ${D}${includedir}/tensorflow/lite
	install -d ${D}${prefix}/local/bin/${PN}-${PV}/tools

	install -m 0644 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/lib/*               ${D}${libdir}

	cd ${S}/tensorflow/lite
	cp --parents $(find . -name "*.h*") ${D}${includedir}/tensorflow/lite

	install -m 0555 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/bin/minimal         ${D}${prefix}/local/bin/${PN}-${PV}/tools
	install -m 0555 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/bin/benchmark_model ${D}${prefix}/local/bin/${PN}-${PV}/tools

	# tensorflow-lite python3 interpreter
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime

	install -m 0644 ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/build/lib.*/tflite_runtime/*             ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime
}

PACKAGES_remove = "${PN}"
RDEPENDS_${PN}-dev = ""

PACKAGES += "${PN}-tools python3-${PN}"

FILES_${PN}-tools = "${prefix}/local/bin/${PN}-${PV}/tools/*"

FILES_python3-${PN} = "${PYTHON_SITEPACKAGES_DIR}/tflite_runtime"

RDEPENDS_python3-${PN} += " python3-ctypes python3-numpy "

PROVIDES += " tensorflow-lite-staticdev python3-tensorflow-lite "
