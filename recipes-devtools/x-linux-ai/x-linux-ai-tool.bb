DESCRIPTION = "Verify X-LINUX-AI version, features and application"
AUTHOR = "STMicroelectronics"
SUMMARY = "X-LINUX-AI version binary"
LICENSE = "CLOSED"

SRC_URI = "	file://x-linux-ai-tool/x-linux-ai-tool.cc \
		file://x-linux-ai-tool/README_sym \
		file://x-linux-ai-tool/Makefile \
"

BBCLASSEXTEND = " nativesdk "

S = "${WORKDIR}/${BPN}"

do_configure() {
    xpkg_ver=$(sed -n -e 's/^.*\(X-LINUX-AI v\)/\1/p' README_sym | cut -f2 -d ' ')
    xpkg_fea=$(sed -n '/^*.TensorFlow/,${p;/^*.Application.samples/q}' README_sym | sed '$d' | sed '1s/^//;$!s/$/ \\n \\/;$s/$//')
    xpkg_app=$(sed -n '/^*.Application.samples/,${p;/^*.Application.support/q}' README_sym | sed '1d;$d' | sed '1s/^//;$!s/$/ \\n \\/;$s/$//' | sed 's/^\ *//g' )
    cat << EOF > readme.h
#include <iostream>
const std::string README_VERSION = "${xpkg_ver}";
const std::string README_FEATURES = "${xpkg_fea}";
const std::string README_APPLI = "${xpkg_app}";
EOF
}

EXTRA_OEMAKE = 'SYSROOT="${RECIPE_SYSROOT}"'

do_install() {
	install -d ${D}/usr/bin
	install -m 0755 ${S}/x-linux-ai	${D}/usr/bin
}

do_install:append:class-nativesdk () {
	install -d ${D}${SDKPATHNATIVE}/usr/bin
	install -m 0755 ${S}/x-linux-ai ${D}${SDKPATHNATIVE}/usr/bin/x-linux-ai
}


FILES:${PN} += "/usr/bin"

FILES:${PN}:append:class-nativesdk = " ${SDKPATHNATIVE}/bin/x-linux-ai "

INSANE_SKIP:${PN} = "ldflags"
