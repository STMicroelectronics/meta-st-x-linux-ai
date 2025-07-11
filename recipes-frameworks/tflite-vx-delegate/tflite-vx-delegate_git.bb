# Copyright 2020-2021 STMicroelectronics
DESCRIPTION = "Verisilicon TFLite VX Delegate for STM32 Devices"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=7d6260e4f3f6f85de05af9c8f87e6fb5"

SRCBRANCH_vx = "main"
SRCREV_vx = "3f4759e07221e6b90bd82989b65febdaaf9ec0ea"

SRCBRANCH_tf = "r2.18"
SRCREV_tf = "500e03dc2d086c8078c0da575932ae5177da22d9"

SRCREV_FORMAT ="vx_tf"

SRC_URI =  "git://github.com/VeriSilicon/tflite-vx-delegate.git;branch=${SRCBRANCH_vx};name=vx;destsuffix=git_vx/;protocol=https \
            git://github.com/tensorflow/tensorflow;branch=${SRCBRANCH_tf};name=tf;destsuffix=git_tf/;protocol=https "
SRC_URI += "file://0001-TFLite-cmake-support-git-clone-shallow-with-specifie.patch;patchdir=${WORKDIR}/git_tf "
SRC_URI += "file://0002-TFLite-align-protobuf-version-on-the-native-one.patch;patchdir=${WORKDIR}/git_tf "
SRC_URI += "file://0003-cmakelist-generate-vx_custom_op-shared-lib-instead-o.patch;patchdir=${WORKDIR}/git_vx "

PV = "2.16.2+git${SRCREV_vx}"
S = "${WORKDIR}/git_vx"
COMPATIBLE_MACHINE = "stm32mp2common"

inherit cmake python3-dir
DEPENDS += "tim-vx patchelf-native flatbuffers-native protobuf-native"

python () {
    #Get major of the PV variable
    version = d.getVar('PV')
    version = version.split("+")
    version_base = version[0]
    version = version_base.split(".")
    major = version[0]
    d.setVar('MAJOR', major)
    d.setVar('PVB', version_base)
}

do_configure[network] = "1"

do_configure:prepend() {
    if [ -n "${http_proxy}" ]; then
        export HTTP_PROXY=${http_proxy}
        export http_proxy=${http_proxy}
    fi
    if [ -n "${https_proxy}" ]; then
        export HTTPS_PROXY=${https_proxy}
        export https_proxy=${https_proxy}
    fi
    unset FC
}

EXTRA_OECMAKE += " -DFETCHCONTENT_SOURCE_DIR_TENSORFLOW=${WORKDIR}/git_tf \
                   -DTIM_VX_INSTALL=${STAGING_DIR_TARGET}/usr \
                   -DTFLITE_ENABLE_XNNPACK=OFF \
                   -DTFLITE_ENABLE_EXTERNAL_DELEGATE=ON \
                   -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
                   -DTFLITE_HOST_TOOLS_DIR=${RECIPE_SYSROOT_NATIVE}/ \
"

do_install() {
    # Install libvx_delegate.so into libdir
    install -d ${D}${libdir}
    install -d ${D}${includedir}/VX
    install -d ${D}${includedir}/tensorflow/compiler/mlir/lite

    install -m 0755 ${WORKDIR}/build/libvx_delegate.so ${D}${libdir}/libvx_delegate.so.${PVB}
    patchelf --set-soname libvx_delegate.so ${D}${libdir}/libvx_delegate.so.${PVB}
    ln -sf libvx_delegate.so.${PVB} ${D}${libdir}/libvx_delegate.so.${MAJOR}
    ln -sf libvx_delegate.so.${PVB} ${D}${libdir}/libvx_delegate.so

    # Install custom op lib
    install -m 0755 ${WORKDIR}/build/libvx_custom_op.so ${D}${libdir}/libvx_custom_op.so
    install -m 0644 ${S}/vsi_npu_custom_op.h ${D}${includedir}/VX/vsi_npu_custom_op.h
}

FILES_SOLIBSDEV = ""

FILES:${PN} += " ${libdir}/libvx_delegate.so.${MAJOR} \
                 ${libdir}/libvx_delegate.so.${PVB} \
                 ${libdir}/libvx_custom_op.so \
"
FILES:${PN}-dev += " ${includedir}/VX/vsi_npu_custom_op.h \
                     ${libdir}/libvx_delegate.so \
"

RDEPENDS:${PN} += " \
    tensorflow-lite \
    ${PYTHON_PN}-tensorflow-lite \
    tensorflow-lite-tools \
    tim-vx \
"

INSANE_SKIP:${PN} += "dev-so"