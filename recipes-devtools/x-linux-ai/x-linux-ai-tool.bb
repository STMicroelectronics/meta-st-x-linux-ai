DESCRIPTION = "X-LINUX-AI tool manager"
AUTHOR = "STMicroelectronics"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI = " file://x-linux-ai-tool/LICENSE \
            file://x-linux-ai-tool/x-linux-ai-tool.cc \
            file://x-linux-ai-tool/README_sym \
            file://x-linux-ai-tool/Makefile \
            file://x-linux-ai-tool/x_linux_ai_installation.service \
            file://x-linux-ai-tool/x_linux_ai_installation.py \
"

inherit systemd
BBCLASSEXTEND = " nativesdk "

S = "${WORKDIR}/${BPN}"

do_configure() {
    # Get package version
    xpkg_ver=$(sed -n 's/^.*X-LINUX-AI version: //p' README_sym)

    # Get features
    xpkg_fea=$(sed -n '/^*.TensorFlow/,${p;/^*.Application.samples/q}' README_sym | sed '$d' | sed '1s/^//;$!s/$/ \\n \\/;$s/$//')

    # Get applications
    xpkg_app=$(sed -n '/^*.Application.samples/,${p;/^*.Application.support/q}' README_sym | sed '1d;$d' | sed '1s/^//;$!s/$/ \\n \\/;$s/$//' | sed 's/^\ *//g' )

    # Get wiki link
    xpkg_link=$(grep -o 'https://wiki.st.com/stm32mp.*Starter_package>' README_sym | sed 's/>$//')

    cat << EOF > readme.h

#include <iostream>
const std::string README_VERSION = "${xpkg_ver}";
const std::string README_FEATURES = "${xpkg_fea}";
const std::string README_APPLI = "${xpkg_app}";
const std::string WIKI_LINK = "${xpkg_link}";
EOF
}

do_install() {
    install -d ${D}${bindir}
    install -d ${D}${systemd_system_unitdir}
    install -m 0755 ${S}/x_linux_ai_installation.py ${D}${bindir}/pkg-install-ai
    install -m 0755 ${S}/x-linux-ai ${D}${bindir}
    install -m 644 ${S}/x_linux_ai_installation.service ${D}${systemd_system_unitdir}
}

FILES:${PN} = "${bindir} ${systemd_system_unitdir} "

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "x_linux_ai_installation.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

RDEPENDS:${PN} += "python3-core"
