DESCRIPTION = "X-LINUX-AI Application manager"
AUTHOR = "STMicroelectronics"
SUMMARY = "X-LINUX-AI-APPLICATION binary"

LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI = " file://x-linux-ai-application/LICENSE                         \
            file://x-linux-ai-application/x-linux-ai-application.cc       \
            file://x-linux-ai-application/x-linux-ai-application-utils.cc \
            file://x-linux-ai-application/x-linux-ai-application.h        \
            file://x-linux-ai-application/Makefile                        \
"
BBCLASSEXTEND = " nativesdk "

S = "${WORKDIR}/${BPN}"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/x-linux-ai-application ${D}${bindir}
}

INSANE_SKIP:${PN} = "ldflags"

EXTRA_OEMAKE += 'STM32MPU="${BOARD_USED}"'