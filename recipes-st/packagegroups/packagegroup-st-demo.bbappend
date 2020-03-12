RDEPENDS_${PN} += "\
    ${@bb.utils.contains('DISTRO_FEATURES', 'wayland', 'tflite-cv-apps-image-classification', '', d)} \
    ${@bb.utils.contains('DISTRO_FEATURES', 'wayland', 'tflite-cv-apps-object-detection', '', d)} \
    "
