SUMMARY = "A date and time library based on C++11/14/17."
AUTHOR = "Howard Hinnant"
HOMEPAGE = "https://github.com/HowardHinnant/date.git"
SECTION = "libs"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=b5d973344b3c7bbf7535f0e6e002d017"

SRC_URI = " \
	git://github.com/HowardHinnant/date.git;protocol=https;branch=master \
	file://date.pc \
    "

S = "${WORKDIR}/git"
PV = "3.0.1"
SRCREV = "6e921e1b1d21e84a5c82416ba7ecd98e33a436d0"

inherit cmake

DEPENDS = "curl"

EXTRA_OECMAKE += " \
	-DBUILD_TZ_LIB=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DUSE_SYSTEM_TZ_DB=ON \
"

do_install:append() {
	# source lacks pkgconfig support. Include a pc file, so 'date' can be found using pkgconfig
	install -d ${D}${libdir}/pkgconfig
	install -m 0644 ${WORKDIR}/date.pc ${D}${libdir}/pkgconfig
}

