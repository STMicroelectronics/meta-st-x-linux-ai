DESCRIPTION = "TensorFlow Lite is an open source deep learning framework for \
on-device inference."
AUTHOR = "Google Inc. and Yuan Tang"
SUMMARY = "TensorFlow Lite Python interpreter and C++ Library"
HOMEPAGE = "https://www.tensorflow.org/lite"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=4158a261ca7f2525513e31ba9c50ae98"

PV = "2.8.0+git${SRCPV}"

SRCREV = "3f878cff5b698b82eea85db2b60d65a2e320850e"
SRC_URI = " git://github.com/tensorflow/tensorflow;branch=r2.8 "
SRC_URI += " file://0001-TFLite-cmake-support-cross-compilation-configuration.patch "
SRC_URI += " file://0002-TFLite-cmake-generate-benchmark_model-binary.patch "
SRC_URI += " file://0003-TFlite-cmake-introduce-shared-library-build.patch "
SRC_URI += " file://0004-TFLite-cmake-support-git-clone-shallow-with-specifie.patch "

S = "${WORKDIR}/git"

python () {
    #Get major of the PV variable
    version = d.getVar('PV')
    version = version.split("+")
    version_base = version[0]
    version = version_base.split(".")
    major = version[0]
    d.setVar('MAJOR', major)
    d.setVar('PVB', version_base)
}

inherit setuptools3

DEPENDS = "zlib \
	   python3-numpy-native \
	   python3-pybind11-native \
	   python3-wheel-native \
	   python3-pybind11 \
	   python3-numpy \
	   swig-native \
	   python3 \
	   gzip-native \
	   cmake-native \
	  "

# Set building environment variables
TENSORFLOW_TARGET="${@bb.utils.contains('TARGET_OS', 'linux-gnueabi', 'linux', '', d)}"
TENSORFLOW_TARGET_ARCH_armv7ve="${@bb.utils.contains('TUNE_FEATURES', 'cortexa7', 'armv7l', '', d)}"

do_compile() {
	# Used to download cmake dependencies when behind a proxy
	if [ -n "${http_proxy}" ]; then
		export HTTP_PROXY=${http_proxy}
	fi
	if [ -n "${https_proxy}" ]; then
		export HTTPS_PROXY=${https_proxy}
	fi

	export PYTHON_TARGET_INCLUDE=${RECIPE_SYSROOT}${includedir}/${PYTHON_DIR}
	export NUMPY_TARGET_INCLUDE=${RECIPE_SYSROOT}${PYTHON_SITEPACKAGES_DIR}/numpy/core/include
	export PYBIND11_TARGET_INCLUDE=${RECIPE_SYSROOT}${PYTHON_SITEPACKAGES_DIR}/pybind11/include
	export TENSORFLOW_TARGET=${TENSORFLOW_TARGET}
	export TENSORFLOW_TARGET_ARCH=${TENSORFLOW_TARGET_ARCH}
	export ARMCC_PREFIX=${CCACHE}${HOST_PREFIX}
	export ARMCC_FLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"

	unset PYTHON
	bash ${S}/tensorflow/lite/tools/pip_package/build_pip_package_with_cmake.sh
}

do_install() {
	# Install tensorflow-lite dynamic library
	install -d ${D}${libdir}
	install -m 0644 ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/cmake_build/libtensorflow-lite.so ${D}${libdir}/libtensorflow-lite.so.${PVB}

	ln -sf libtensorflow-lite.so.${PVB} ${D}${libdir}/libtensorflow-lite.so.${MAJOR}
	ln -sf libtensorflow-lite.so.${PVB} ${D}${libdir}/libtensorflow-lite.so

	# Install benchmark_model
	install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	chrpath -d ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/cmake_build/tools/benchmark/benchmark_model
	install -m 0755 ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/cmake_build/tools/benchmark/benchmark_model ${D}${prefix}/local/bin/${PN}-${PVB}/tools

	# Install header files
	install -d ${D}${includedir}/tensorflow
	install -d ${D}${includedir}/tensorflow/lite
	cd ${S}/tensorflow/lite
	cp --parents $(find . -name "*.h*" -not -path "*cmake_build/*") ${D}${includedir}/tensorflow/lite
	cd ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/cmake_build
	cp --parents $(find . -name "*.h*") ${D}${includedir}/tensorflow/lite

	# Install Tflite python3 interpreter
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime
	install -m 0644  ${S}/tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/tflite_runtime/* ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime
}

PACKAGES += "${PN}-tools python3-${PN}"

FILES_${PN}  = "${libdir}/libtensorflow-lite.so.${MAJOR}"
FILES_${PN} += "${libdir}/libtensorflow-lite.so.${PVB}"

FILES_${PN}-tools = "${prefix}/local/bin/${PN}-${PVB}/tools/*"

FILES_python3-${PN} = "${PYTHON_SITEPACKAGES_DIR}/tflite_runtime"

RDEPENDS_python3-${PN} += " python3-ctypes python3-numpy "

PROVIDES += "python3-${PN}"
