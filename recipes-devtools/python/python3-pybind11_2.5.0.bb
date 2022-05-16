SUMMARY = "Seamless operability between C++11 and Python"
HOMEPAGE = "https://github.com/wjakob/pybind11"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=beb87117af69fd10fbf9fb14c22a2e62"

SRC_URI = "git://github.com/pybind/pybind11.git;protocol=https;branch=master"
SRCREV = "3b1dbebabc801c9cf6f0953a4c20b904d444f879"

inherit setuptools3

S = "${WORKDIR}/git"

BBCLASSEXTEND = "native"
