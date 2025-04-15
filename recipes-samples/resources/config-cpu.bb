# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing configuration file for X-LINUX-AI CPU version"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://resources-files/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "resources-files/LICENSE"
LICENSE:${PN} = "SLA0044"

PV="6.0.1"

SRC_URI += "	file://resources-files/config_board_cpu.sh \
				file://resources-files/LICENSE \
				"

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install (){
	install -d ${D}${prefix}/local/x-linux-ai/resources/
	install -m 0755 ${S}/resources-files/config_board_cpu.sh  ${D}${prefix}/local/x-linux-ai/resources/
}

FILES:${PN} += "${prefix}/local/x-linux-ai/resources/"
RDEPENDS:${PN} += " bash "
RCONFLICTS:${PN} = "config-npu"