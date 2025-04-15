# Copyright (C) 2019, STMicroelectronics - All Rights Reserved
SUMMARY = "Create package containing all resources need by out of the box applications"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://resources-files/LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "resources-files/LICENSE"
LICENSE:${PN} = "SLA0044"

PV="6.0.1"

SRC_URI = "	file://resources-files "

S = "${WORKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -d ${D}${prefix}/local/x-linux-ai/resources/

	# install all stai resources
	install -m 0644 ${S}/resources-files/stai_*.png 				${D}${prefix}/local/x-linux-ai/resources

	# install all configuration scripts
	install -m 0755 ${S}/resources-files/check_camera_preview.sh 	${D}${prefix}/local/x-linux-ai/resources
	install -m 0755 ${S}/resources-files/setup_camera.sh 			${D}${prefix}/local/x-linux-ai/resources
	install -m 0644 ${S}/resources-files/Default.css 				${D}${prefix}/local/x-linux-ai/resources
	# install all common resources
	install -m 0644 ${S}/resources-files/label_*.png 				${D}${prefix}/local/x-linux-ai/resources
	install -m 0644 ${S}/resources-files/exit_*.png 				${D}${prefix}/local/x-linux-ai/resources

	# install all IC resources
	install -m 0644 ${S}/resources-files/IC_*.png 					${D}${prefix}/local/x-linux-ai/resources

	# install all OD resources
	install -m 0644 ${S}/resources-files/OD_*.png 					${D}${prefix}/local/x-linux-ai/resources

	# install all FR resources
	install -m 0644 ${S}/resources-files/FR_*.png 					${D}${prefix}/local/x-linux-ai/resources

}

do_install:append:stm32mp2common(){
	# overwrite camera setup script for stm32mp2x
    install -m 0755 ${S}/resources-files/check_camera_preview_main_isp.sh     ${D}${prefix}/local/x-linux-ai/resources/check_camera_preview.sh
    install -m 0755 ${S}/resources-files/setup_camera_main_isp.sh             ${D}${prefix}/local/x-linux-ai/resources/setup_camera.sh

	# install all stai resources for OVX logos
	install -m 0644 ${S}/resources-files/stai_mpu_ovx*.png 				      ${D}${prefix}/local/x-linux-ai/resources

	# install all SEMANTIC SEGMENTATION resources
	install -m 0644 ${S}/resources-files/SEMSEG_*.png 						  ${D}${prefix}/local/x-linux-ai/resources

	# install all POSE ESTIMATION resources
	install -m 0644 ${S}/resources-files/PE_*.png 							  ${D}${prefix}/local/x-linux-ai/resources
}

FILES:${PN} += "${prefix}/local/"

RDEPENDS:${PN} += " bash "
