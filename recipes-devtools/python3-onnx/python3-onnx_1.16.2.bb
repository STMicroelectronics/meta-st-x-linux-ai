SUMMARY = "Open Neural Network Exchange"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"

python () {
    #Get major of the PV variable
    python_version = d.getVar('PYTHON_BASEVERSION')
    python_version = python_version.replace(".", "")
    d.setVar('PYTHON_PV', python_version)
}

PYPI_PACKAGE = "onnx"
PYPI_PACKAGE_EXT = "whl"
PYPI_TARGET_ARCH = "manylinux_2_17_aarch64.manylinux2014_aarch64"
PYPI_CPYTHON_VER = "cp${PYTHON_PV}"
PYPI_SHA256SUM = "${@'8f/3d/6d623912bd7262abba8f7d1b2930896c8ccc3e11eda668b27d28e43c7705' if d.getVar('DISTRO_CODENAME') == 'scarthgap' \
            else '82/fc/04b03e31b6741c3b430d04cfa055660242eba800e15c1c3394db3082098d'}"
PYPI_ONNX_WHEEL = "${PYPI_PACKAGE}-${PV}-${PYPI_CPYTHON_VER}-${PYPI_CPYTHON_VER}-${PYPI_TARGET_ARCH}.${PYPI_PACKAGE_EXT}"
PYPI_SRC_URI = "https://files.pythonhosted.org/packages/${PYPI_SHA256SUM}/${PYPI_ONNX_WHEEL}"

DEPENDS += "python3-pip-native \
            python3-wheel-native"

COMPATIBLE_MACHINE = "stm32mp2common"

inherit pypi python3-dir python3native

SRC_URI += " file://LICENSE "
SRC_URI[sha256sum] = "${@'9b77a6c138f284dfc9b06fa370768aa4fd167efc49ff740e2158dd02eedde8d0' if d.getVar('DISTRO_CODENAME') == 'scarthgap' \
				else '39a57d196fe5d73861e70d9625674e6caf8ca13c5e9c740462cf530a07cd2e1c'}"

S = "${WORKDIR}"

do_install(){
    install -d ${D}/${PYTHON_SITEPACKAGES_DIR}

    ${STAGING_BINDIR_NATIVE}/pip3 install --disable-pip-version-check -v --platform manylinux2014_aarch64 \
        -t ${D}/${PYTHON_SITEPACKAGES_DIR} --no-cache-dir --no-deps \
        ${S}/${PYPI_ONNX_WHEEL}
}

FILES:${PN} += "${libdir}/python*"

RDEPENDS:${PN} = " \
    ${PYTHON_PN}-numpy \
    ${PYTHON_PN}-protobuf \
    ${PYTHON_PN}-typing-extensions \
"