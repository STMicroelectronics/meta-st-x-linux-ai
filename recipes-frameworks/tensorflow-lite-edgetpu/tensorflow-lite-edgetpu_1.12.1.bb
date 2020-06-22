DESCRIPTION = "TensorFlow Lite package supporting and including EdgeTPU"
SUMMARY = "TensorFlow Lite Python interpreter and C++ Library for Coral Edge TPU"
HOMEPAGE = "https://coral.ai/products/accelerator"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

S = "${WORKDIR}/git"

SRC_URI += " git://github.com/tensorflow/tensorflow.git;name=tensorflow;branch=master;protocol=https"
SRCREV_tensorflow  = "d855adfc5a0195788bf5f92c3c7352e638aa1109"

# Patch to be applied
SRC_URI += " file://0001-TFLite-tools-make-remove-hash-and-flags-files-from-t.patch "
SRC_URI += " file://0002-TFLite-tools-make-add-fftsg2d.c-file-in-the-build-re.patch "
SRC_URI += " file://0003-TFLite-tools-make-remove-ruy-tune_tool.cc-from-the-b.patch "
SRC_URI += " file://0004-TFLite-pip-package-support-cross-compilation-environ.patch "
SRC_URI += " file://0005-Add-sparsity-to-Makefile-sources.patch "
SRC_URI += " file://0006-Modify-experimental-resource-path-in-Makefile.patch "
SRC_URI += " file://0007-Align-interpreter-library-to-tflite-edgetpu-runtime.patch "

SRC_URI += " git://github.com/google-coral/edgetpu.git;name=edgetpu_runtime;protocol=https;destsuffix=edgetpu_runtime"
SRCREV_edgetpu_runtime = "285cacf0a879e3e343abd38c33eb622ae253529c"

inherit setuptools3

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
	export TENSORFLOW_TARGET=${TENSORFLOW_TARGET}
	export TENSORFLOW_TARGET_ARCH=${TENSORFLOW_TARGET_ARCH}
	export TENSORFLOW_CC_PREFIX=${CCACHE}${HOST_PREFIX}
	export TENSORFLOW_EXTRA_CFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"
	export TENSORFLOW_EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"

	unset PYTHON
	bash ${S}/tensorflow/lite/tools/pip_package/build_pip_package.sh
}

do_install(){
	install -d ${D}${libdir}
	install -d ${D}${includedir}/tensorflow
	install -d ${D}${includedir}/tensorflow/lite
	install -d ${D}${sysconfdir}/udev/rules.d
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_edgetpu_runtime

	install -m 0644 ${S}/tensorflow/lite/tools/make/gen/${TENSORFLOW_TARGET}_${TENSORFLOW_TARGET_ARCH}/lib/*   ${D}${libdir}

	cd ${S}/tensorflow/lite
	cp --parents $(find . -name "*.h*")                                                                        ${D}${includedir}/tensorflow/lite

	# tensorflow-lite python3 interpreter
	install -m 0644 ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/build/lib.*/tflite_runtime/* ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_edgetpu_runtime

	# Install the edgge TPU udev rules
	install -m 0666 ${S}/../edgetpu_runtime/libedgetpu/edgetpu-accelerator.rules                               ${D}${sysconfdir}/udev/rules.d/99-edgetpu-accelerator.rules

	# Installing the Edge TPU Shared Library - Throttled version"
	install -m 0755 ${S}/../edgetpu_runtime/libedgetpu/throttled/armv7a/libedgetpu.so.1.0                      ${D}${libdir}/libedgetpu-throttled.so.1.0

	# Installing the Edge TPU Shared Library - Max Frequency version"
	install -m 0755 ${S}/../edgetpu_runtime/libedgetpu/direct/armv7a/libedgetpu.so.1.0                         ${D}${libdir}/libedgetpu-max.so.1.0

	ln -sf libedgetpu-max.so.1.0 ${D}${libdir}/libedgetpu.so.1.0
	ln -sf libedgetpu.so.1.0 ${D}${libdir}/libedgetpu.so.1
	ln -sf libedgetpu.so.1.0 ${D}${libdir}/libedgetpu.so

	# Installing the edgetpu header
	cp ${S}/../edgetpu_runtime/libedgetpu/edgetpu.h                                                            ${D}${includedir}/tensorflow/lite
}

FILES_${PN}  = "${sysconfdir}"
FILES_${PN} += "${libdir}/libedgetpu-throttled.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu-max.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu.so.1"
FILES_${PN} += "${libdir}/libedgetpu.so"

FILES_${PN}-dev = "${libdir}/libedgetpu.so ${includedir}"

INHIBIT_PACKAGE_STRIP = "1"

PACKAGES += "python3-${PN}"

# avoid to rename the package
DEBIAN_NOAUTONAME_python3-${PN} = "1"
FILES_python3-${PN} = "${PYTHON_SITEPACKAGES_DIR}/tflite_edgetpu_runtime"

RDEPENDS_${PN} += "libusb1"
RDEPENDS_python3-${PN} += " ${PN} python3-ctypes python3-numpy "

PROVIDES += " tensorflow-lite-edgetpu-staticdev python3-tensorflow-lite-edgetpu "

INSANE_SKIP_${PN} = "already-stripped"
