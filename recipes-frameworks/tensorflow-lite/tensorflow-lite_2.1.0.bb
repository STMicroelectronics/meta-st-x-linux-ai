DESCRIPTION = "TensorFlow Lite Python interpreter and C++ Library"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=64a34301f8e355f57ec992c2af3e5157"

SRC_URI = " https://github.com/tensorflow/tensorflow/archive/v${PV}.tar.gz;downloadfilename=tensorflow-lite-v${PV}.tar.gz "
SRC_URI[md5sum] = "269414a50b46bb676a0ef9e611839528"
SRC_URI[sha256sum] = "638e541a4981f52c69da4a311815f1e7989bf1d67a41d204511966e1daed14f7"

# Patch to be applied
SRC_URI += " file://0001-TFLite-fix-eigen-url-in-download_dependencies.sh-scr.patch "
SRC_URI += " file://0002-TFLite-tools-make-remove-hash-and-flags-files-from-t.patch "
SRC_URI += " file://0003-TFLite-tools-make-add-fftsg2d.c-file-in-the-build-re.patch "
SRC_URI += " file://0004-TFLite-tools-make-remove-ruy-tune_tool.cc-from-the-b.patch "
SRC_URI += " file://0005-TFLite-pip-package-support-cross-compilation-environ.patch "

S = "${WORKDIR}/tensorflow-${PV}"

inherit setuptools3

PACKAGES += "${PN}-examples python3-${PN}"

DEPENDS = " \
	coreutils-native \
	curl-native \
	gzip-native \
	unzip-native \
	swig-native \
	python3-wheel-native \
	python3-numpy-native \
	python3 \
	zlib \
"

RDEPENDS_${PN}-python3 += " \
	python3-numpy \
	python3-pillow \
"

#Recipe only tested with armv7ve
COMPATIBLE_HOST = "(arm.*).*-linux"
COMPATIBLE_HOST_armv4  = 'null'
COMPATIBLE_HOST_armv5  = 'null'
COMPATIBLE_HOST_armv6  = 'null'
COMPATIBLE_HOST_aarch64_class-target = 'null'

do_configure(){
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
TENSORFLOW_TARGET_ARCH_armv7ve="${@bb.utils.contains('TUNE_FEATURES', 'armv7ve', 'armv7l', '', d)}"

do_compile () {
	export TENSORFLOW_CROSS_COMPILATION="true"
	export TENSORFLOW_TARGET=${TENSORFLOW_TARGET}
	export TENSORFLOW_TARGET_ARCH=${TENSORFLOW_TARGET_ARCH}
	export TENSORFLOW_CC_PREFIX=${CCACHE}${HOST_PREFIX}
	export TENSORFLOW_EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"

	bash ${S}/tensorflow/lite/tools/pip_package/build_pip_package.sh
}

do_install(){
	# tensorflow-lite static lib for dev and examples
	install -d ${D}${libdir}
	install -d ${D}${includedir}/tensorflow
	install -d ${D}${includedir}/tensorflow/lite
	install -d ${D}${bindir}/${PN}-${PV}/examples

	install -m install -m 0644 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/lib/* ${D}${libdir}

	cd ${S}/tensorflow/lite
	cp --parents $(find . -name "*.h*") ${D}${includedir}/tensorflow/lite

	install -m 0555 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/bin/minimal         ${D}${bindir}/${PN}-${PV}/examples
	install -m 0555 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/bin/benchmark_model ${D}${bindir}/${PN}-${PV}/examples

	# tensorflow-lite python3 interpreter
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime

	install -m 0644 ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/build/lib.*/tflite_runtime/*                     ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime
}

ALLOW_EMPTY_${PN} = "1"

FILES_${PN} = ""

FILES_${PN}-staticdev = " \
	${includedir} \
	${libdir}/*.a \
"

FILES_${PN}-examples = " \
	${bindir}/${PN} \
	${bindir}/${PN}-${PV}/examples/minimal \
	${bindir}/${PN}-${PV}/examples/benchmark_model \
"

FILES_python3-${PN} = " \
	${PYTHON_SITEPACKAGES_DIR}/tflite_runtime \
"

PROVIDES += "tensorflow-lite-staticdev"
PROVIDES += "python3-tensorflow-lite"
