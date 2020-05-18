DESCRIPTION = "The Arm Compute Library contains a comprehensive collection of \
software functions implemented for the Arm Cortex-A family of CPU processors \
and the Arm Mali family of GPUs. It is a convenient repository of low-level \
optimized functions that developers can source individually or use as part of \
complex pipelines in order to accelerate their algorithms and applications."
SUMMARY = "Arm Compute Library (ACL)"
HOMEPAGE = "https://developer.arm.com/ip-products/processors/machine-learning/compute-library"
LICENSE = "MIT"

LIC_FILES_CHKSUM = "file://LICENSE;md5=2c2e6902c16b52c68b379cecc3fafad7"

SRC_URI = " https://github.com/ARM-software/ComputeLibrary/archive/v${PV}.tar.gz;downloadfilename=arm-compute-library-v${PV}.tar.gz "
SRC_URI[md5sum] = "84c0e218e36c8cde90f1a7a2f0f98e9d"
SRC_URI[sha256sum] = "319cc145e8b99efbdee2a70a8b39885a3cf6b8612f39bc9ac96a7666f9a588e0"

S = "${WORKDIR}/ComputeLibrary-${PV}"

inherit scons

OESCONS_COMMON_FLAG  = " toolchain_prefix="${CCACHE}${HOST_PREFIX}" "
OESCONS_COMMON_FLAG += " extra_cxx_flags="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}" "
OESCONS_COMMON_FLAG += " extra_link_flags="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}" "
OESCONS_COMMON_FLAG += " neon=1 "
OESCONS_COMMON_FLAG += " opencl=0 "
OESCONS_COMMON_FLAG += " set_soname=1 "
OESCONS_COMMON_FLAG += " benchmark_tests=1 "
OESCONS_COMMON_FLAG += " validation_tests=0 "

EXTRA_OESCONS_arm = "arch=armv7a ${OESCONS_COMMON_FLAG}"
EXTRA_OESCONS_aarch64 = "arch=arm64-v8a ${OESCONS_COMMON_FLAG}"

do_compile_prepend() {
	unset CC CXX
}

do_install() {
	install -d ${D}${libdir}
	install -d ${D}${bindir}/${PN}-${PV}/tools
	install -d ${D}${bindir}/${PN}-${PV}/tools/examples
	install -d ${D}${includedir}/${BPN}

	# install dynamic libraries
	cp -P ${S}/build/*.so                                   ${D}${libdir}
	cp -P ${S}/build/*.so*                                  ${D}${libdir}

	# install static libraries
	install -m 0644 ${S}/build/*.a                          ${D}${libdir}

	# install example and benchmark binaries
	install -m 0555 ${S}/build/examples/*                   ${D}${bindir}/${PN}-${PV}/tools/examples
	rm ${D}${bindir}/${PN}-${PV}/tools/examples/*.o

	install -m 0555 ${S}/build/tests/arm_compute_benchmark  ${D}${bindir}/${PN}-${PV}/tools
	install -m 0555 ${S}/build/tests/benchmark_graph*       ${D}${bindir}/${PN}-${PV}/tools
	install -m 0555 ${S}/build/tests/benchmark_neon*        ${D}${bindir}/${PN}-${PV}/tools
	rm ${D}${bindir}/${PN}-${PV}/tools/*.o

	# install include files
	cp -rf ${S}/arm_compute                                 ${D}${includedir}/${BPN}
	cp -rf ${S}/include                                     ${D}${includedir}/${BPN}
	cp -rf ${S}/support                                     ${D}${includedir}/${BPN}
}

PACKAGES += "${PN}-tools"

FILES_${PN}-tools = "${bindir}/${PN}-${PV}/tools/*"

FILES_${PN} = "${libdir}"

INSANE_SKIP_${PN} = "ldflags"
INSANE_SKIP_${PN}-tools = "ldflags"
