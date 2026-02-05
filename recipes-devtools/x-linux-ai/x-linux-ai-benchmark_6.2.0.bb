DESCRIPTION = "X-LINUX-AI NN unified benchmark"
AUTHOR = "STMicroelectronics"
SUMMARY = "X-LINUX-AI NN unified benchmark binaries"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI = " file://x-linux-ai-benchmark/LICENSE \
            file://x-linux-ai-benchmark/x_linux_ai_benchmark_mp1 \
            file://x-linux-ai-benchmark/x_linux_ai_benchmark_mp2 \
"

S = "${WORKDIR}/${BPN}"

inherit python3-dir

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp2common = "stm32mp2_npu"

do_install() {
    install -d ${D}/usr/bin
    install -m 0755 ${S}/x_linux_ai_benchmark_mp1 ${D}/usr/bin/x-linux-ai-benchmark
}

do_install:append:stm32mp2common(){
    install -m 0755 ${S}/x_linux_ai_benchmark_mp2 ${D}/usr/bin/x-linux-ai-benchmark
}

FILES:${PN} += "/usr/bin"

RDEPENDS:${PN} += " \
    libpython3 \
    ${PYTHON_PN}-prettytable \
"
