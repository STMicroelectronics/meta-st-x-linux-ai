DESCRIPTION = "The Arm Compute Library contains a comprehensive collection of \
software functions implemented for the Arm Cortex-A family of CPU processors \
and the Arm Mali family of GPUs. It is a convenient repository of low-level \
optimized functions that developers can source individually or use as part of \
complex pipelines in order to accelerate their algorithms and applications."
SUMMARY = "Arm Compute Library (ACL)"
HOMEPAGE = "https://developer.arm.com/ip-products/processors/machine-learning/compute-library"
LICENSE = "MIT"

LIC_FILES_CHKSUM = "file://LICENSE;md5=9598101cf48c5f479cfda9f3fc6fc566"

SRC_URI = " git://github.com/ARM-software/ComputeLibrary;protocol=https"
SRCREV = "f7399fd616cc8ccedfae668da3023e589a7ef558"
S = "${WORKDIR}/git"

inherit scons

OESCONS_COMMON_FLAG  = " toolchain_prefix="${CCACHE}${HOST_PREFIX}" "
OESCONS_COMMON_FLAG += " extra_cxx_flags="${TARGET_CC_ARCH} -march=armv7ve ${TOOLCHAIN_OPTIONS} -fPIC" "
OESCONS_COMMON_FLAG += " extra_link_flags="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}" "
OESCONS_COMMON_FLAG += " neon=1 "
OESCONS_COMMON_FLAG += " opencl=0 "
OESCONS_COMMON_FLAG += " set_soname=1 "
OESCONS_COMMON_FLAG += " benchmark_tests=1 "
OESCONS_COMMON_FLAG += " validation_tests=0 "

# Selection of arch=armv7a allow to select 32 bits system.
# The -march is overide by the extra_cxx_flags configuration to match armv7ve
EXTRA_OESCONS = "arch=armv7a ${OESCONS_COMMON_FLAG}"

do_compile_prepend() {
	unset CC CXX
}

do_install() {
	install -d ${D}${libdir}
	install -d ${D}${prefix}/local/bin/${PN}-${PV}/tools
	install -d ${D}${prefix}/local/bin/${PN}-${PV}/tools/examples
	install -d ${D}${includedir}/${BPN}

	# install dynamic libraries
	cp -P ${S}/build/*.so                                   ${D}${libdir}
	cp -P ${S}/build/*.so*                                  ${D}${libdir}

	# install static libraries
	install -m 0644 ${S}/build/*.a                          ${D}${libdir}

	# install example and benchmark binaries
	install -m 0555 ${S}/build/examples/*                   ${D}${prefix}/local/bin/${PN}-${PV}/tools/examples
	rm ${D}${prefix}/local/bin/${PN}-${PV}/tools/examples/*.o

	install -m 0555 ${S}/build/tests/arm_compute_benchmark  ${D}${prefix}/local/bin/${PN}-${PV}/tools

	# install include files
	cp -rf ${S}/arm_compute                                 ${D}${includedir}/${BPN}
	cp -rf ${S}/include                                     ${D}${includedir}/${BPN}
	cp -rf ${S}/support                                     ${D}${includedir}/${BPN}
}

PACKAGES += "${PN}-tools"

FILES_${PN}-tools = "${prefix}/local/bin/${PN}-${PV}/tools/*"

FILES_${PN} = "${libdir}"

INSANE_SKIP_${PN} = "ldflags"
INSANE_SKIP_${PN}-tools = "ldflags"
