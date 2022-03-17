DESCRIPTION = "Edge TPU library for Coral device."
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"

PV = "2.8.0+git${SRCPV}"

SRCREV = "ea1eaddbddece0c9ca1166e868f8fd03f4a3199e"
SRC_URI = "git://github.com/google-coral/libedgetpu.git "
SRC_URI += " file://0001-update-make_build-Makefile-to-accept-cross-compilati.patch "
SRC_URI += " file://0002-fix-build-issue-with-gcc-v11.x.patch "

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
	abseil-cpp \
	flatbuffers-native \
	flatbuffers \
	tensorflow-lite \
	libusb1 \
	vim-native \
"

EXTRA_OEMAKE  = 'EXTRA_CFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'
EXTRA_OEMAKE += 'EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'
EXTRA_OEMAKE += 'TFROOT="${RECIPE_SYSROOT}/usr/include/"'
EXTRA_OEMAKE += 'LIBEDGETPU_VERSION="${PVB}"'
EXTRA_OEMAKE += 'LIBEDGETPU_VERSION_MAJOR="${MAJOR}"'

do_compile () {
	#Check if abseil-cpp version use static or shared libraries
	ABSL_SHARED_LIB=0
	FILE=${RECIPE_SYSROOT}/${libdir}/pkgconfig/absl_status.pc
	if [ -f "$FILE" ]; then
		ABSL_SHARED_LIB=1
	fi

	oe_runmake ABSL_SHARED_LIB=${ABSL_SHARED_LIB} -C ${S} -f makefile_build/Makefile
}

do_install () {
	install -d ${D}${libdir}
	install -d ${D}${includedir}/tensorflow/lite
	install -d ${D}${sysconfdir}/udev/rules.d

	# Installing the Edge TPU shared library version"
	install -m 0755 ${S}/out/lib/*.so* ${D}${libdir}

	# Install the edgge TPU udev rules
	install -m 0666 ${S}/debian/edgetpu-accelerator.rules ${D}${sysconfdir}/udev/rules.d/99-edgetpu-accelerator.rules
	# Add MODE="0666" to the udev rules to access edgetpu for non root users
	sed -i s/$/',MODE="0666"'/ ${D}${sysconfdir}/udev/rules.d/99-edgetpu-accelerator.rules

	ln -sf libedgetpu-max.so.${PVB} ${D}${libdir}/libedgetpu-max.so.${MAJOR}
	ln -sf libedgetpu-std.so.${PVB} ${D}${libdir}/libedgetpu-std.so.${MAJOR}
	ln -sf libedgetpu-std.so.${PVB} ${D}${libdir}/libedgetpu.so.${PVB}
	ln -sf libedgetpu.so.${PVB} ${D}${libdir}/libedgetpu.so.${MAJOR}
	ln -sf libedgetpu.so.${PVB} ${D}${libdir}/libedgetpu.so

	# Installing the edgetpu header
	cp ${S}/tflite/public/edgetpu.h ${D}${includedir}/tensorflow/lite
}

FILES:${PN} += "${sysconfdir}"
FILES:${PN} += "${libdir}/libedgetpu-max.so.${PVB}"
FILES:${PN} += "${libdir}/libedgetpu-std.so.${PVB}"
FILES:${PN} += "${libdir}/libedgetpu.so.${PVB}"
FILES:${PN} += "${libdir}/libedgetpu.so.${MAJOR}"
FILES:${PN} += "${libdir}/libedgetpu-max.so.${MAJOR}"
FILES:${PN} += "${libdir}/libedgetpu-std.so.${MAJOR}"

FILES:${PN}-dev  = "${libdir}/libedgetpu.so"
FILES:${PN}-dev += "${includedir}/tensorflow/lite"

INSANE_SKIP:${PN} = "ldflags"

RDEPENDS:${PN} += "libusb1 flatbuffers tensorflow-lite"
