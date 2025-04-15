# Copyright (C) 2024, STMicroelectronics - All Rights Reserved

SUMMARY = "X-LINUX-AI tool manager"
DESCRIPTION = "X-LINUX-AI is a free of charge open-source software \
package dedicated to AI. It is a complete ecosystem that allow \
developers working with OpenSTLinux to create AI-based application \
very easily."

HOMEPAGE = "https://github.com/stmicroelectronics/meta-st-x-linux-ai"

LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI = " file://x-linux-ai-tool/LICENSE \
            file://x-linux-ai-tool/x-linux-ai-tool.cc \
            file://x-linux-ai-tool/README_sym \
            file://x-linux-ai-tool/Makefile \
"

BBCLASSEXTEND = " nativesdk "

S = "${WORKDIR}/${BPN}"

do_configure() {
    # Get package version
    xpkg_ver=$(sed -n 's/^.*X-LINUX-AI version: //p' README_sym)

    # Get features
    xpkg_fea=$(sed -n '/^* AI Frameworks:/,/^* Out of the box applications:/{/^* Out of the box applications:/!p}' README_sym | sed '$d' | sed '1d; $!s/$/ \\n \\/')

    # Get applications
    xpkg_app=$(sed -n '/^* Out of the box applications:/,/^* Utilities:/{/^* Utilities:/!p}' README_sym | sed '$d' | sed '1d; $!s/$/ \\n \\/')

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
    install -m 0755 ${S}/x-linux-ai ${D}${bindir}
}

FILES:${PN} = "${bindir}"