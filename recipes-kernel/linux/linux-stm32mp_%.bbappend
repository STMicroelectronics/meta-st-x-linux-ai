FILESEXTRAPATHS:prepend := "${THISDIR}:"

SRC_URI += "file://${LINUX_VERSION}/0001-media-videobuf2-dma-sg-pages-allocation-using-GFP_DM.patch \
"

PR = "r2"
