DESCRIPTION = "Edge TPU library for Coral device."
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"

PV = "2.8.0+git${SRCPV}"

SRCREV = "ea1eaddbddece0c9ca1166e868f8fd03f4a3199e"
SRC_URI = "git://github.com/google-coral/libedgetpu.git "
SRC_URI += " file://0001-update-make_build-Makefile-to-accept-cross-compilati.patch "

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

do_configure () {
	#Set the version of the libraries according to the version of this recipe
	sed -i "s#%%VERSION%%#${PVB}#g" ${S}/makefile_build/Makefile
}

EXTRA_OEMAKE  = 'EXTRA_CFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'
EXTRA_OEMAKE += 'EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"'
EXTRA_OEMAKE += 'TFROOT="${RECIPE_SYSROOT}/usr/include/"'

do_compile () {
	oe_runmake -C ${S} -f makefile_build/Makefile
}

do_install () {
	install -d ${D}${libdir}
	install -d ${D}${includedir}/tensorflow/lite
	install -d ${D}${sysconfdir}/udev/rules.d

	# Installing the Edge TPU shared library version"
	install -m 0755 ${S}/out/lib/*.so*                     ${D}${libdir}

	# Install the edgge TPU udev rules
	install -m 0666 ${S}/debian/edgetpu-accelerator.rules  ${D}${sysconfdir}/udev/rules.d/99-edgetpu-accelerator.rules

	ln -sf libedgetpu-max.so.${PVB} ${D}${libdir}/libedgetpu-max.so.${MAJOR}
	ln -sf libedgetpu-std.so.${PVB} ${D}${libdir}/libedgetpu-std.so.${MAJOR}
	ln -sf libedgetpu-std.so.${PVB} ${D}${libdir}/libedgetpu.so.${PVB}
	ln -sf libedgetpu.so.${PVB} ${D}${libdir}/libedgetpu.so.${MAJOR}
	ln -sf libedgetpu.so.${PVB} ${D}${libdir}/libedgetpu.so

	# Installing the edgetpu header
	cp ${S}/tflite/public/edgetpu.h         ${D}${includedir}/tensorflow/lite
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

RDEPENDS:${PN} += "libusb1 flatbuffers"
