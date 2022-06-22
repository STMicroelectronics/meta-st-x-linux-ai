DESCRIPTION = "ONNX Runtime is a cross-platform, high performance machine learning inferencing framework"
AUTHOR = "Microsoft"
SUMMARY = "ONNX Runtime Python package & C++ library"
HOMEPAGE = "https://www.onnxruntime.ai/"
LICENSE = "MIT"

LIC_FILES_CHKSUM = "file://LICENSE;md5=0f7e3b1308cb5c00b372a6e78835732d"

PV = "1.11.0+git${SRCPV}"

SRCREV = "b713855a980056d89a1e550ad81dc3c19573d7a0"
SRC_URI = "gitsm://github.com/microsoft/onnxruntime.git;branch=rel-1.11.0;protocol=https"
SRC_URI += "file://0001-remove-AVX-specific-micro-benchmark.patch"
SRC_URI += "file://0002-onnxruntime-add-SONAME-with-MAJOR-version.patch"
SRC_URI += "file://0003-onnxruntime-libcustom_op_library-remove-relative-path.patch"

S = "${WORKDIR}/git"
B = "${S}/cmake/build"

inherit python3-dir

DEPENDS = "\
	python3-numpy-native \
	python3-pybind11 \
	python3-native \
	python3 \
	cmake-native \
	protobuf-native \
	protobuf \
	flatbuffers-native \
	nsync \
	boost \
	re2 \
	date \
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

set_proxy() {
	if [ -n "${http_proxy}" ]; then
		export HTTP_PROXY=${http_proxy}
	fi
	if [ -n "${https_proxy}" ]; then
		export HTTPS_PROXY=${https_proxy}
	fi
}

do_configure:prepend() {
	set_proxy
}
do_compile:prepend() {
	set_proxy
}

do_configure[network] = "1"
do_configure() {
	mkdir -p ${B}
	cd ${B}

	cmake -DCMAKE_SYSTEM_NAME=Linux \
	      -DCMAKE_SYSTEM_VERSION=1 \
	      -DCMAKE_SYSTEM_PROCESSOR=armv7 \
	      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	      -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	      -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
	      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
	      -DCMAKE_BUILD_TYPE=Release \
	      -DCMAKE_INSTALL_PREFIX="${prefix}" \
	      -Donnxruntime_BUILD_SHARED_LIB=ON \
	      -Donnxruntime_BUILD_BENCHMARKS=ON \
	      -DCMAKE_PREFIX_PATH="${STAGING_LIBDIR};${STAGING_INCDIR};${STAGING_INCDIR}/${PYTHON_DIR}" \
	      -DONNX_CUSTOM_PROTOC_EXECUTABLE="${STAGING_BINDIR_NATIVE}/protoc" \
	      -Donnxruntime_PREFER_SYSTEM_LIB=ON \
	      -Donnxruntime_ENABLE_PYTHON=ON \
	      -DPYTHON_EXECUTABLE="${STAGING_BINDIR_NATIVE}/${PYTHON_PN}-native/${PYTHON_PN}" \
	      -DPython_EXECUTABLE="${STAGING_BINDIR_NATIVE}/${PYTHON_PN}-native/${PYTHON_PN}" \
	      -DPYTHON_LIBRARY="${STAGING_LIBDIR}" \
	      -Dpybind11_INCLUDE_DIR="${STAGING_INCDIR}/${PYTHON_DIR}/pybind11" \
	      -Dpybind11_DIR="${STAGING_DATADIR}/cmake/pybind11" \
	      -Ddate_DIR="${STAGING_LIBDIR}/cmake/date" \
	      -Dre2_DIR="${STAGING_LIBDIR}/cmake/re2" \
	      -DONNXRUNTIME_VERSION_MAJOR=${MAJOR} ..
}

do_install() {
	# Install the base components.
	export DESTDIR="${D}"
	oe_runmake install

	# This shared lib is used by onnxruntime_shared_lib_test and onnxruntime_test_python.py
	install -m 644 ${B}/libcustom_op_library.so ${D}${libdir}
	# And this one only by onnxruntime_test_python.py
	install -m 644 ${B}/libtest_execution_provider.so ${D}${libdir}

	# Install the symlinks.
	ln -sf libonnxruntime.so.${PVB} ${D}${libdir}/libonnxruntime.so.${MAJOR}
	ln -sf libonnxruntime.so.${PVB} ${D}${libdir}/libonnxruntime.so

	# Move the onnx_test_runner executable that was installed in /usr instead of /usr/local.
	install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	mv ${D}${bindir}/onnx_test_runner ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	# Now the bin directory is empty, which will make the qa tests fail, so we remove it.
	rm -r ${D}${bindir}

	# These are not included in the base installation, so we install them manually.
	install -m 755 ${B}/onnxruntime_perf_test ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/onnxruntime_test_all ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/onnxruntime_shared_lib_test ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/onnxruntime_api_tests_without_env ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/onnxruntime_global_thread_pools_test ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/onnxruntime_test_python.py ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	install -m 755 ${B}/helper.py ${D}${prefix}/local/bin/${PN}-${PVB}/tools
	cp -r ${B}/testdata ${D}${prefix}/local/bin/${PN}-${PVB}/tools

	# We have to change some of the RPATH as well.
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/onnxruntime_perf_test
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/onnxruntime_shared_lib_test
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/onnxruntime_api_tests_without_env
	chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/onnxruntime_global_thread_pools_test
	chrpath -r '$ORIGIN' ${D}${libdir}/libtest_execution_provider.so

	# Install the Python package.
	mkdir -p ${D}${PYTHON_SITEPACKAGES_DIR}/onnxruntime
	cp -r ${B}/onnxruntime ${D}${PYTHON_SITEPACKAGES_DIR}
}

# The package_qa() task does not like the fact that this library is present in both onnxruntime-tools
# and python3-onnxruntime packages (the normal /usr/lib version and a copy placed inside the Python package).
# So we simply mark the lib as a "private lib", to prevent the task from outputting an error.
PRIVATE_LIBS = "libonnxruntime_providers_shared.so"

PACKAGES += "${PYTHON_PN}-${PN} ${PN}-tools"
PROVIDES += "${PYTHON_PN}-${PN} ${PN}-tools"

FILES:${PN} = "${libdir}/pkgconfig/* ${libdir}/libonnxruntime_providers_shared.so ${libdir}/libonnxruntime.so.*"
FILES:${PN}-dev = "${libdir}/libonnxruntime.so ${includedir}/*"
FILES:${PN}-tools = "${prefix}/local/bin/${PN}-${PVB}/tools/* ${libdir}/libcustom_op_library.so ${libdir}/libtest_execution_provider.so"
FILES:${PYTHON_PN}-${PN} = "${PYTHON_SITEPACKAGES_DIR}/onnxruntime/*"

# onnxruntime_test_python.py requires python3-numpy.
RDEPENDS:${PN}-tools += "${PYTHON_PN}-numpy"

RRECOMMENDS:${PN} = "${PYTHON_PN}-onnx"
