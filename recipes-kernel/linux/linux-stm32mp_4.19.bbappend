FILESEXTRAPATHS_prepend_stm32mpcommon := "${THISDIR}/${PN}:"

# Add the configuration to support usb serial
KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/fragments/4.19/fragment-00-usbserial.config"

SRC_URI += "file://4.19/fragment-00-usbserial.config;subdir=fragments"
