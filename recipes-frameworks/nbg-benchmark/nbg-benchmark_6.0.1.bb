# COPYRIGHT (C) 2023, STMICROELECTRONICS - All Rights Reserved
SUMMARY = "NBG Benchmark tool to parse and benchmark nbg model files using OpenVX API"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

COMPATIBLE_MACHINE = "stm32mp2common"

DEPENDS += "jpeg"
DEPENDS:append:stm32mp2common = " gcnano-driver-stm32mp gcnano-userland "

PV = "6.0.1"

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

SRC_URI = " file://LICENSE \
            file://nbg_benchmark.cc \
            file://vnn_utils.cc \
            file://vnn_utils.h \
            file://Makefile"

S = "${WORKDIR}"

do_configure() {
	oe_runmake -C ${S}/ clean
}

EXTRA_OEMAKE  = 'SYSROOT="${RECIPE_SYSROOT}"'

do_compile(){
    export SYSROOT=${RECIPE_SYSROOT}
	oe_runmake -C ${S}/ all 'CC=${CC}'
}

do_install() {
	install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    install -m 0755 ${S}/nbg_benchmark ${D}${prefix}/local/bin/${PN}-${PVB}/tools
}

INSANE_SKIP:${PN} = "ldflags"
FILES:${PN} += "${prefix}/local/bin/${PN}-${PVB}/tools/nbg_benchmark"
RDEPENDS:${PN} += " x-linux-ai-benchmark "
