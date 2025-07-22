FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " \
    file://1001-uvcsink-backport-uvcsink-plugin-from-gstreamer-1.26..patch \
    file://1002-uvcsink-Respond-to-control-requests-with-proper-erro.patch \
    file://1003-uvcsink-define-gst_util_simplify_fraction-from-gstut.patch \
    file://1004-uvcsink-respond-to-control-setup-event-to-avoid-infi.patch \
"

PR = "r2"

PACKAGECONFIG[uvcsink] = "-Duvcgadget=enabled,-Duvcgadget=disabled,libgudev"

PACKAGECONFIG += " uvcsink "
