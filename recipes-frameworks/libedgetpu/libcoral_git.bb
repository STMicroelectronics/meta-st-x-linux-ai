DESCRIPTION = "libcoral C++ API, which provides convenient functions to \
perform inferencing and on-device transfer learning with TensorFlow Lite \
models on Coral devicesEdge TPU library for Coral device."
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=d8927f3331d2b3e321b7dd1925166d25"

# Release Grouper
PV = "2.0.0+git${SRCPV}"

SRCREV = "6589d0bb49c7fdbc4194ce178d06f8cdc7b5df60"
SRC_URI = "git://github.com/google-coral/libcoral.git;protocol=https;branch=master "
SRC_URI += "file://0001-Makefile-to-support-Yocto-build-system.patch"
SRC_URI += "file://0002-align-signature-API-with-TFLite-2.8.0.patch"
SRC_URI += "file://0003-make-sure-only-MPL2.0-or-more-permissively-licensed-.patch"
SRC_URI += "file://0004-add-glog-library-dependency.patch"

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

DEPENDS = " \
	libedgetpu \
	glog \
	abseil-cpp \
	libeigen \
	tensorflow-lite \
	flatbuffers \
"

EXTRA_OEMAKE  = 'EXTRA_CFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'
EXTRA_OEMAKE += 'EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'
EXTRA_OEMAKE += 'FLATBUFFERS_INCL="${RECIPE_SYSROOT}/usr/include/"'
EXTRA_OEMAKE += 'EIGEN_INCL="${RECIPE_SYSROOT}/usr/include/eigen3"'
EXTRA_OEMAKE += 'LIBCORAL_VERSION="${PVB}"'
EXTRA_OEMAKE += 'LIBCORAL_VERSION_MAJOR="${MAJOR}"'

do_compile () {
	oe_runmake -C ${S} -f makefile_build/Makefile
}

do_install () {
	install -d ${D}${libdir}
	install -d ${D}${includedir}/coral

	# Installing the Edge TPU shared library version"
	install -m 0755 ${S}/out/lib/*.so* ${D}${libdir}

	ln -sf libcoral.so.${PVB} ${D}${libdir}/libcoral.so.${MAJOR}
	ln -sf libcoral.so.${PVB} ${D}${libdir}/libcoral.so

	# Install header files
	cd ${S}/coral
	cp --parents $(find . -name "*.h*" -not -path "*tools/*" -not -path "*examples/*" | grep -v test | grep -v benchmark) ${D}${includedir}/coral
}

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += "libedgetpu glog"
