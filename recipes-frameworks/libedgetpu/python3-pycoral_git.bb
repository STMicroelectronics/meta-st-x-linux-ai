DESCRIPTION = "easy-to-use Python API that helps you run inferences and \
perform on-device transfer learning with TensorFlow Lite models on Coral \
devices."
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=d8927f3331d2b3e321b7dd1925166d25"

# Release Grouper
PV = "2.0.0+git${SRCPV}"

SRCREV = "9972f8ec6dbb8b2f46321e8c0d2513e0b6b152ce"
SRC_URI = "git://github.com/google-coral/pycoral.git;protocol=https;branch=master "
SRC_URI += "file://0001-Makefile-to-support-Yocto-build-system.patch"
SRC_URI += "file://0002-setup.py-requires-tflite-runtime-2.8.0.patch"
SRC_URI += "file://0003-Update-major-number-of-libedgetpu-from-1-to-2.patch"

S = "${WORKDIR}/git"

DEPENDS = " \
	libcoral \
	libedgetpu \
	tensorflow-lite \
	abseil-cpp \
	libeigen \
	python3-numpy \
	python3-pybind11 \
"

inherit setuptools3

S = "${WORKDIR}/git"

EXTRA_OEMAKE += 'PYCORAL_TARGET_ARCH="armv7l"'
EXTRA_OEMAKE += 'PYTHON_TARGET_INCLUDE="${RECIPE_SYSROOT}${includedir}/${PYTHON_DIR}"'
EXTRA_OEMAKE += 'NUMPY_TARGET_INCLUDE="${RECIPE_SYSROOT}${PYTHON_SITEPACKAGES_DIR}/numpy/core/include"'
EXTRA_OEMAKE += 'EIGEN_TARGET_INCLUDE="${RECIPE_SYSROOT}/usr/include/eigen3"'
EXTRA_OEMAKE += 'EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'

do_compile:prepend () {
	oe_runmake -C ${S} -f makefile_build/Makefile
}

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += "libcoral python3-tensorflow-lite python3-pillow"
