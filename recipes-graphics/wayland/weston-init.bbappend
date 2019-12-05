FILESEXTRAPATHS_prepend_stm32mpcommon := "${THISDIR}/${PN}:"

SRC_URI_append_stm32mpcommon = " file://weston.ini "

do_install_append () {
    install -m 0644 ${WORKDIR}/weston.ini ${D}${sysconfdir}/xdg/weston
}
