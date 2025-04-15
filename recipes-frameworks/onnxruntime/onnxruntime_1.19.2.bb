DESCRIPTION = "ONNX Runtime is a cross-platform, high performance machine learning inferencing framework"
AUTHOR = "Microsoft"
SUMMARY = "ONNX Runtime Python package & C++ library"
HOMEPAGE = "https://www.onnxruntime.ai/"
LICENSE = "MIT"

LIC_FILES_CHKSUM = "file://LICENSE;md5=0f7e3b1308cb5c00b372a6e78835732d"

PV = "1.19.2+git${SRCPV}"

SRCREV = "ffceed9d44f2f3efb9dd69fa75fea51163c91d91"
SRC_URI = "gitsm://github.com/microsoft/onnxruntime.git;branch=rel-1.19.2;protocol=https"
SRC_URI += " file://0001-onnxruntime-test-remove-AVX-specific-microbenchmark.patch "
SRC_URI += " file://0002-onnxruntime-add-SONAME-with-MAJOR-version.patch "
SRC_URI += " file://0003-onnxruntime-test-libcustom-library-remove-relative.patch "
SRC_URI += " file://0004-onnxruntime-fix-incompatibility-with-compiler-GCC12..patch "
SRC_URI += " file://0009-remove-ENV-variable-that-is-not-usefull.patch "
SRC_URI += " file://0008-onnxruntime-cmake-change-visibility-compilation-opti.patch "
SRC_URI += " file://0011-onnxruntime-Split-Pad-and-some-element-wise-OPs-support.patch "
SRC_URI += " file://0012-onnxruntime-VSINPU-EP-Add-VSINPU-EP-to-support-python-bindings.patch "
SRC_URI:append:stm32mp2common = " file://0010-fix-uncompatible-cmake-flag-issue.patch"
SRC_URI:append:stm32mp2common = " file://0007-onnxruntime-xnnpack-Fix-mcpu-compiler-build-failure.patch "

PROTOC_VERSION = "21.12"
SRC_URI += "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VERSION}/protoc-${PROTOC_VERSION}-linux-x86_64.zip;name=protoc;subdir=protoc-${PROTOC_VERSION}/"
SRC_URI[protoc.sha256sum] = "3a4c1e5f2516c639d3079b1586e703fc7bcfa2136d58bda24d1d54f949c315e8"

S = "${WORKDIR}/git"

inherit python3-dir cmake

DEPENDS = "\
	${PYTHON_PN}-numpy-native \
	${PYTHON_PN}-numpy \
	${PYTHON_PN}-pybind11 \
	${PYTHON_PN}-native \
	${PYTHON_PN} \
	"
DEPENDS:append:stm32mp2common = " tim-vx "

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

OECMAKE_SOURCEPATH = "${S}/cmake"

EXTRA_OECMAKE += "    -DCMAKE_BUILD_TYPE=Release \
		      -DCMAKE_INSTALL_PREFIX="${prefix}" \
		      -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
		      -Donnxruntime_BUILD_SHARED_LIB=ON \
		      -Donnxruntime_BUILD_BENCHMARKS=ON \
		      -DCMAKE_PREFIX_PATH="${STAGING_LIBDIR};${STAGING_INCDIR};${STAGING_INCDIR}/${PYTHON_DIR}" \
		      -DONNX_CUSTOM_PROTOC_EXECUTABLE="${WORKDIR}/protoc-${PROTOC_VERSION}/bin/protoc" \
		      -Donnxruntime_PREFER_SYSTEM_LIB=OFF \
		      -Donnxruntime_ENABLE_PYTHON=ON \
		      -DPYTHON_EXECUTABLE="${STAGING_BINDIR_NATIVE}/${PYTHON_PN}-native/${PYTHON_PN}" \
		      -DPython_EXECUTABLE="${STAGING_BINDIR_NATIVE}/${PYTHON_PN}-native/${PYTHON_PN}" \
		      -DPYTHON_LIBRARY="${STAGING_LIBDIR}" \
		      -DPython_NumPy_INCLUDE_DIR="${STAGING_LIBDIR}/${PYTHON_DIR}/site-packages/numpy/core/include" \
		      -Dpybind11_INCLUDE_DIR="${STAGING_INCDIR}/${PYTHON_DIR}/pybind11" \
		      -DONNXRUNTIME_VERSION_MAJOR=${MAJOR}  \
		      -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
		      -Donnxruntime_USE_XNNPACK=ON \
		      -DTIM_VX_INSTALL=${RECIPE_SYSROOT}/usr \
		      -Donnxruntime_BUILD_UNIT_TESTS=ON \
"
EXTRA_OECMAKE:append:stm32mp2common = "-Donnxruntime_USE_VSINPU=ON "

ONNX_TARGET_ARCH:armv7ve="${@bb.utils.contains('TUNE_FEATURES', 'cortexa7', 'armv7ve', '', d)}"
ONNX_TARGET_ARCH:armv7a="${@bb.utils.contains('TUNE_FEATURES', 'cortexa7', 'armv7a', '', d)}"
ONNX_TARGET_ARCH:aarch64="${@bb.utils.contains('TUNE_FEATURES', 'cortexa35', 'aarch64', '', d)}"

do_generate_toolchain_file:append() {
	echo "set( CMAKE_SYSTEM_PROCESSOR ${ONNX_TARGET_ARCH} )" >> ${WORKDIR}/toolchain.cmake
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
}

do_compile:prepend() {
    if [ -n "${http_proxy}" ]; then
        export HTTP_PROXY=${http_proxy}
        export http_proxy=${http_proxy}
    fi
    if [ -n "${https_proxy}" ]; then
        export HTTPS_PROXY=${https_proxy}
        export https_proxy=${https_proxy}
    fi
}

do_install() {

	# Install onnxruntime dynamic library
	install -d ${D}${libdir}
	install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -d ${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests

	install -m 0644 ${B}/libonnxruntime.so				 ${D}${libdir}/libonnxruntime.so.${PVB}

	# This shared lib is used by onnxruntime_shared_lib_test and onnxruntime_test_python.py
	install -m 644 ${B}/libcustom_op_library.so			 ${D}${libdir}

	# And this one only by onnxruntime_test_python.py
	install -m 644 ${B}/libtest_execution_provider.so		${D}${libdir}
	install -m 644 ${B}/libonnxruntime_providers_shared.so	${D}${libdir}/libonnxruntime_providers_shared.so
	install -m 644 ${B}/libcustom_op_invalid_library.so		${D}${libdir}/libcustom_op_invalid_library.so
	install -m 644 ${B}/onnxruntime_pybind11_state.so		${D}${libdir}/onnxruntime_pybind11_state.so

	# Install the symlinks.
	ln -sf libonnxruntime.so.${PVB} ${D}${libdir}/libonnxruntime.so.${MAJOR}
	ln -sf libonnxruntime.so.${PVB} ${D}${libdir}/libonnxruntime.so

	# These are not included in the base installation, so we install them manually.
	install -m 755 ${B}/onnx_test_runner						${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests
	install -m 755 ${B}/onnxruntime_perf_test					${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/onnxruntime_test_all					${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests
	install -m 755 ${B}/onnxruntime_shared_lib_test				${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests
	install -m 755 ${B}/onnxruntime_global_thread_pools_test	${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests
	install -m 755 ${B}/onnxruntime_test_python.py				${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests
	install -m 755 ${B}/helper.py								${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests
	cp -r ${B}/testdata											${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests

	# We have to change some of the RPATH as well.
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/onnxruntime_perf_test
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests/onnxruntime_shared_lib_test
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/unit-tests/onnxruntime_global_thread_pools_test
	chrpath -r '$ORIGIN' ${D}${libdir}/libtest_execution_provider.so

	# Install the Python package.
	mkdir -p ${D}${PYTHON_SITEPACKAGES_DIR}/onnxruntime
	cp -r    ${B}/onnxruntime ${D}${PYTHON_SITEPACKAGES_DIR}
	find ${D}${PYTHON_SITEPACKAGES_DIR} -name "libonnx*.so*" -exec rm {} \;

    # Remove the static library from the Python package installation
    rm -f ${D}${PYTHON_SITEPACKAGES_DIR}/onnxruntime/capi/libonnxruntime_providers_vsinpu.a

	# Install header files
	install -d ${D}${includedir}/onnxruntime
	install -d ${D}${includedir}/onnxruntime/core/providers/
	cd ${S}/onnxruntime
	cp --parents $(find . -name "*.h*" -not -path "*cmake_build/*") 	${D}${includedir}/onnxruntime
	cp  ${S}/include/onnxruntime/core/session/onnxruntime_cxx_api.h  	${D}${includedir}/onnxruntime
	cp  ${S}/include/onnxruntime/core/session/onnxruntime_c_api.h  		${D}${includedir}/onnxruntime
	cp  ${S}/include/onnxruntime/core/session/onnxruntime_cxx_inline.h  ${D}${includedir}/onnxruntime
	cp  ${S}/include/onnxruntime/core/session/onnxruntime_float16.h  	${D}${includedir}/onnxruntime
	cp  ${S}/include/onnxruntime/core/session/onnxruntime_session_options_config_keys.h  	${D}${includedir}/onnxruntime
	cp -r  ${S}/include/onnxruntime/core/providers/* 	${D}${includedir}/onnxruntime/core/providers/
}

# The package_qa() task does not like the fact that this library is present in both onnxruntime-tools
# and python3-onnxruntime packages (the normal /usr/lib version and a copy placed inside the Python package).
# So we simply mark the lib as a "private lib", to prevent the task from outputting an error.
PRIVATE_LIBS = "libonnxruntime_providers_shared.so"

PACKAGES += "${PYTHON_PN}-${PN} ${PN}-tools ${PN}-unit-tests"
PROVIDES += "${PYTHON_PN}-${PN} ${PN}-tools ${PN}-unit-tests"
INSANE_SKIP_${PYTHON_PN}-${PN} += "staticdev"

FILES:${PN} = "${libdir}/pkgconfig/* ${libdir}/libonnxruntime_providers_shared.so ${libdir}/libonnxruntime.so.* ${libdir}/onnxruntime_pybind11_state.so"
FILES:${PN}-dev = "${libdir}/libonnxruntime.so ${includedir}/*"
FILES:${PN}-tools = "${prefix}/local/bin/${PN}-${PVB}/tools/onnxruntime_perf_test"
FILES:${PN}-unit-tests = "${prefix}/local/bin/${PN}-${PVB}/unit-tests/* ${libdir}/libcustom_op_invalid_library.so ${libdir}/libtest_execution_provider.so ${libdir}/libcustom_op_library.so"
FILES:${PYTHON_PN}-${PN} = "${PYTHON_SITEPACKAGES_DIR}/onnxruntime/*"

# onnxruntime_test_python.py unitary test requires python3-numpy and python3-onnxruntime packages
RDEPENDS:${PN}-unit-tests += "${PYTHON_PN}-${PN}"
RDEPENDS:${PN}-unit-tests:append:stm32mp2common = " tim-vx-tools "
RDEPENDS:${PN} += " x-linux-ai-benchmark "
RDEPENDS:${PN}-tools += "onnxruntime"
RDEPENDS:${PYTHON_PN}-${PN} += "${PYTHON_PN}-numpy onnxruntime"