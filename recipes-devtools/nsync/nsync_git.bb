DESCRIPTION = "nsync is a C library that exports various synchronization primitives, such as mutexes."
AUTHOR = "Google"
SUMMARY = "nsync: a C library for synchronization primitives."
HOMEPAGE = "https://github.com/google/nsync"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"

PV = "master+git${SRCPV}"

SRCREV = "712e51fc50fee7c609a6ed38639288bd6030e876"
SRC_URI = "git://github.com/google/nsync.git;branch=master;protocol=https"

S = "${WORKDIR}/git"
B = "${S}"

inherit cmake
