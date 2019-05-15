PACKAGE_INSTALL_remove += "\
    ${@bb.utils.contains('DISTRO_FEATURES', 'wayland', 'demo-launcher', '', d)} \
    "

PACKAGE_INSTALL += "\
    ${@bb.utils.contains('DISTRO_FEATURES', 'wayland', 'ai-demo-launcher', '', d)} \
    ${@bb.utils.contains('DISTRO_FEATURES', 'wayland', 'tensorflow-lite-applications', '', d)} \
    "
