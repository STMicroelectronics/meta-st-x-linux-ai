DESCRIPTION = "Edge TPU library for Coral device."
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"

SRC_URI = "git://github.com/google-coral/libedgetpu.git "
SRCREV = "14eee1a076aa1af7ec1ae3c752be79ae2604a708"
S = "${WORKDIR}/git"

# Patch to be applied
SRC_URI += " file://0001-add-Makefile.linux-to-build-libedgetpu-from-cross-co.patch "

DEPENDS = " \
	abseil-cpp \
	flatbuffers-native \
	flatbuffers \
	tensorflow-lite \
	libusb1 \
	vim-native \
"

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_configure () {
	#Generate executable_generated.h header
	#use of the option found in libedgetpu/executable/BUILD file
	cd ${S}/executable/
	flatc -c -b executable.fbs --gen-object-api --force-empty --gen-mutable

	#Generate driver_option_generated.h header
	#no specific option found in libedgetpu/api/BUILD file
	cd ${S}/api/
	flatc -c -b driver_options.fbs

	#Generate usb_latest_firmware.h header
	cd ${S}/driver/usb
	OUT="usb_latest_firmware.h"
	echo "namespace {" > ${OUT}
	for FILENAME in *.bin; do
		FILEBASE=$(echo "${FILENAME}" | cut -f 1 -d '.');
		echo "const unsigned char ${FILEBASE} [] = {" >> ${OUT};
		xxd -i < ${FILENAME} >> ${OUT};
		echo "};" >> ${OUT};
		echo "constexpr unsigned int ${FILEBASE}_len = sizeof(${FILEBASE})/sizeof(unsigned char);" >> ${OUT};
		echo "" >> ${OUT};
	done
	echo "} // namespace" >> ${OUT}
}

do_compile () {
	export LIBEDGETPU_EXTRA_CXXFLAGS="${TARGET_CC_ARCH} ${TOOLCHAIN_OPTIONS}"
	oe_runmake -C ${S} -f Makefile.linux
}

do_install () {
	install -d ${D}${libdir}
	install -d ${D}${includedir}/tensorflow/lite
	install -d ${D}${sysconfdir}/udev/rules.d

	# Installing the Edge TPU shared library version"
	install -m 0755 ${S}/build/lib/*.so.1.0                ${D}${libdir}

	# Install the edgge TPU udev rules
	install -m 0666 ${S}/debian/edgetpu-accelerator.rules  ${D}${sysconfdir}/udev/rules.d/99-edgetpu-accelerator.rules

	ln -sf libedgetpu-high.so.1.0 ${D}${libdir}/libedgetpu.so.1.0
	ln -sf libedgetpu.so.1.0 ${D}${libdir}/libedgetpu.so.1
	ln -sf libedgetpu.so.1.0 ${D}${libdir}/libedgetpu.so

	# Installing the edgetpu header
	cp ${S}/tflite/public/edgetpu.h         ${D}${includedir}/tensorflow/lite
}

do_clean () {
	oe_runmake -C ${S} -f Makefile.linux clean
}

FILES_${PN} += "${sysconfdir}"
FILES_${PN} += "${libdir}/libedgetpu-max.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu-high.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu-med.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu-low.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu.so.1.0"
FILES_${PN} += "${libdir}/libedgetpu.so.1"

FILES_${PN}-dev  = "${libdir}/libedgetpu.so"
FILES_${PN}-dev += "${includedir}/tensorflow/lite"

RDEPENDS_${PN} += "libusb1"
