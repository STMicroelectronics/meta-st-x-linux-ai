DESCRIPTION = "TensorFlow Lite is an open source deep learning framework for \
on-device inference."
AUTHOR = "Google Inc. and Yuan Tang"
SUMMARY = "TensorFlow Lite Python interpreter and C++ Library"
HOMEPAGE = "https://www.tensorflow.org/lite"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=4158a261ca7f2525513e31ba9c50ae98"

PV = "2.16.2+git${SRCPV}"

SRCREV = "810f233968cec850915324948bbbc338c97cf57f"

SRC_URI = " git://github.com/tensorflow/tensorflow;protocol=https;branch=r2.16 "
SRC_URI += " file://0001-TFLite-cmake-add-python-numpy-and-pybind11-include-p.patch "
SRC_URI += " file://0002-TFLite-cmake-add-schema_conversion_utils.cc-in-tflit.patch "
SRC_URI += " file://0003-TFLite-cmake-add-SONAME-with-MAJOR-version.patch "
SRC_URI += " file://0004-TFLite-cmake-support-git-clone-shallow-with-specifie.patch "
SRC_URI += " file://0005-TFLite-remove-filter-on-OPT-4bits-sources.patch"
SRC_URI += " file://0006-TFLite-cmake-change-visibility-compilation-options.patch "
SRC_URI:append:stm32mp2common = " file://0007-TFLite-fix-aarch64-support-for-XNNPACK.patch "

S = "${WORKDIR}/git"

inherit setuptools3 cmake

DEPENDS = "zlib \
	   ${PYTHON_PN}-numpy-native \
	   ${PYTHON_PN}-pybind11-native \
	   ${PYTHON_PN}-wheel-native \
	   ${PYTHON_PN}-pybind11 \
	   ${PYTHON_PN}-numpy \
	   swig-native \
	   ${PYTHON_PN} \
	   gzip-native \
	  "

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

# Set building environment variables
TENSORFLOW_TARGET="${@bb.utils.contains('TARGET_OS', 'linux-gnueabi', 'linux', '', d)}"
TENSORFLOW_TARGET_ARCH:armv7ve="${@bb.utils.contains('TUNE_FEATURES', 'cortexa7', 'armv7l', '', d)}"
TENSORFLOW_TARGET:aarch64="linux"
TENSORFLOW_TARGET_ARCH:aarch64="aarch64"

OECMAKE_SOURCEPATH = "${S}/tensorflow/lite"
# Activate -O3 optimization and disable debug symbols
OECMAKE_C_FLAGS_RELEASE = "-O3 -DNDEBUG"
OECMAKE_CXX_FLAGS_RELEASE = "-O3 -DNDEBUG"
# Build tensorflow-lite.so library, _pywrap_tensorflow_interpreter_wrapper.so library and the benchmark_model application
OECMAKE_TARGET_COMPILE =  "tensorflow-lite _pywrap_tensorflow_interpreter_wrapper benchmark_model"

EXTRA_OECMAKE += " -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
		   -DTFLITE_ENABLE_XNNPACK=ON \
		   -DPYTHON_TARGET_INCLUDE=${RECIPE_SYSROOT}${includedir}/${PYTHON_DIR} \
		   -DNUMPY_TARGET_INCLUDE=${RECIPE_SYSROOT}${PYTHON_SITEPACKAGES_DIR}/numpy/core/include \
		   -DPYBIND11_TARGET_INCLUDE=${RECIPE_SYSROOT}${PYTHON_SITEPACKAGES_DIR}/pybind11/include \
		   -DTFLITE_VERSION_MAJOR=${MAJOR}  \
"

do_generate_toolchain_file:append() {
	echo "set( CMAKE_SYSTEM_PROCESSOR ${TENSORFLOW_TARGET_ARCH} )" >> ${WORKDIR}/toolchain.cmake
	sed -i 's/-mthumb//g' ${WORKDIR}/toolchain.cmake
}

do_configure[network] = "1"

do_configure:prepend() {
    if [ -n "${http_proxy}" ]; then
        export HTTP_PROXY=${http_proxy}
        export http_proxy=${http_proxy}
    fi
    if [ -n "${https_proxy}" ]; then
        export HTTPS_PROXY=${https_proxy}
        export https_proxy=${https_proxy}
    fi

    unset PYTHON
    unset FC
}

do_compile[network] = "1"

do_compile:prepend() {
	# Used to download cmake dependencies when behind a proxy
	if [ -n "${http_proxy}" ]; then
		export HTTP_PROXY=${http_proxy}
	fi
	if [ -n "${https_proxy}" ]; then
		export HTTPS_PROXY=${https_proxy}
	fi
}

SETUPTOOLS_SETUP_PATH = "${WORKDIR}/build"

do_compile:append() {
	# Build the python wheel (procedure extract form the build_pip_package_with_cmake.sh)
	BUILD_DIR=${WORKDIR}/build
	TENSORFLOW_DIR=${S}
	TENSORFLOW_LITE_DIR="${TENSORFLOW_DIR}/tensorflow/lite"
	TENSORFLOW_VERSION=$(grep "_VERSION = " "${TENSORFLOW_DIR}/tensorflow/tools/pip_package/setup.py" | cut -d= -f2 | sed "s/[ '-]//g")
	mkdir -p "${BUILD_DIR}/tflite_runtime"
	cp -r "${TENSORFLOW_LITE_DIR}/tools/pip_package/debian" \
	      "${TENSORFLOW_LITE_DIR}/tools/pip_package/MANIFEST.in" \
	      "${BUILD_DIR}"
	cp -r "${TENSORFLOW_LITE_DIR}/python/interpreter_wrapper" "${BUILD_DIR}"
	cp "${TENSORFLOW_LITE_DIR}/tools/pip_package/setup_with_binary.py" "${BUILD_DIR}/setup.py"
	cp "${TENSORFLOW_LITE_DIR}/python/interpreter.py" \
	   "${TENSORFLOW_LITE_DIR}/python/metrics/metrics_interface.py" \
	   "${TENSORFLOW_LITE_DIR}/python/metrics/metrics_portable.py" \
	   "${BUILD_DIR}/tflite_runtime"
	echo "__version__ = '${TENSORFLOW_VERSION}'" >> "${BUILD_DIR}/tflite_runtime/__init__.py"
	echo "__git_version__ = '$(git -C "${TENSORFLOW_DIR}" describe)'" >> "${BUILD_DIR}/tflite_runtime/__init__.py"

	export PACKAGE_VERSION="${TENSORFLOW_VERSION}"
	export PROJECT_NAME="tflite_runtime"
	cp "${BUILD_DIR}/_pywrap_tensorflow_interpreter_wrapper.so" "tflite_runtime"

	setuptools3_do_compile
}

do_install() {
	# Install tensorflow-lite dynamic library
	install -d ${D}${libdir}
	install -m 0644 ${WORKDIR}/build/libtensorflow-lite.so.${MAJOR} ${D}${libdir}/libtensorflow-lite.so.${PVB}

	ln -sf libtensorflow-lite.so.${PVB} ${D}${libdir}/libtensorflow-lite.so.${MAJOR}
	ln -sf libtensorflow-lite.so.${PVB} ${D}${libdir}/libtensorflow-lite.so

	# Install benchmark_model
	install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 0755 ${WORKDIR}/build/tools/benchmark/benchmark_model ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/benchmark_model

	# Install header files
	install -d ${D}${includedir}/tensorflow
	install -d ${D}${includedir}/tensorflow/lite
	cd ${S}/tensorflow/lite
	cp --parents $(find . -name "*.h*" -not -path "*cmake_build/*") ${D}${includedir}/tensorflow/lite
	cd ${WORKDIR}/build
	cp --parents $(find . -name "*.h*") ${D}${includedir}/tensorflow/lite

	# Install Tflite python3 interpreter
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime.egg-info
	install -m 0644  ${WORKDIR}/build/tflite_runtime/* ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime
	install -m 0644  ${WORKDIR}/build/tflite_runtime.egg-info/* ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime.egg-info
	chrpath -r '$ORIGIN' ${D}${PYTHON_SITEPACKAGES_DIR}/tflite_runtime/*.so
}

PACKAGES += "${PN}-tools ${PYTHON_PN}-${PN}"

FILES:${PN}  = "${libdir}/libtensorflow-lite.so.${MAJOR}"
FILES:${PN} += "${libdir}/libtensorflow-lite.so.${PVB}"

FILES:${PN}-tools = "${prefix}/local/bin/${PN}-${PVB}/tools/*"

FILES:${PYTHON_PN}-${PN}  = "${PYTHON_SITEPACKAGES_DIR}/tflite_runtime"
FILES:${PYTHON_PN}-${PN} += "${PYTHON_SITEPACKAGES_DIR}/tflite_runtime.egg-info"

RDEPENDS:${PYTHON_PN}-${PN} += " ${PYTHON_PN}-ctypes ${PYTHON_PN}-numpy "
RDEPENDS:${PN} += " x-linux-ai-benchmark "
RDEPENDS:${PN}:append:stm32mp2common = " tflite-vx-delegate "

PROVIDES += "${PYTHON_PN}-${PN}"
