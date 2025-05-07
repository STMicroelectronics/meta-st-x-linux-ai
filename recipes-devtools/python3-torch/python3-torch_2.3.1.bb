SUMMARY = "Tensors and Dynamic neural networks in Python with strong GPU acceleration"
DESCRIPTION = "PyTorch is a Python package that provides two high-level features: Tensor computation \
(like NumPy) with strong GPU acceleration and Deep neural networks built on a tape-based autograd system"
HOMEPAGE = "https://github.com/pytorch/pytorch"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=825686e962009a3882ebb677af6edd60"

python () {
    #Get major of the PV variable
    python_version = d.getVar('PYTHON_BASEVERSION')
    python_version = python_version.replace(".", "")
    d.setVar('PYTHON_PV', python_version)
}

PYPI_PACKAGE = "torch"
PYPI_PACKAGE_EXT = "whl"
PYPI_TARGET_ARCH = "manylinux2014_aarch64"
PYPI_CPYTHON_VER = "cp${PYTHON_PV}"
PYPI_SHA256SUM = "${@'1b/a1/e8b286b85f19dd701a4b853c0554898b1fa69cea552c7d1ec39bc86f59aa' if d.getVar('DISTRO_CODENAME') == 'scarthgap' \
				else '5c/dc/82b5314ffcffa071440108fdccf59159abcd937b8e4d53f3237914089e60'}"
PYPI_ONNX_WHEEL = "${PYPI_PACKAGE}-${PV}-${PYPI_CPYTHON_VER}-${PYPI_CPYTHON_VER}-${PYPI_TARGET_ARCH}.${PYPI_PACKAGE_EXT}"
PYPI_SRC_URI = "https://files.pythonhosted.org/packages/${PYPI_SHA256SUM}/${PYPI_ONNX_WHEEL}"

DEPENDS += "python3-pip-native \
		   	python3-wheel-native"

COMPATIBLE_MACHINE = "stm32mp2common"

inherit pypi python3-dir python3native

SRC_URI += " file://LICENSE "
SRC_URI[sha256sum] = "${@'224259821fe3e4c6f7edf1528e4fe4ac779c77addaa74215eb0b63a5c474d66c' if d.getVar('DISTRO_CODENAME') == 'scarthgap' \
				else '490cc3d917d1fe0bd027057dfe9941dc1d6d8e3cae76140f5dd9a7e5bc7130ab'}"

S = "${WORKDIR}"

do_install(){
    install -d ${D}/${PYTHON_SITEPACKAGES_DIR}

    ${STAGING_BINDIR_NATIVE}/pip3 install --disable-pip-version-check -v --platform manylinux2014_aarch64 \
        -t ${D}/${PYTHON_SITEPACKAGES_DIR} --no-cache-dir --no-deps \
        ${S}/${PYPI_ONNX_WHEEL}
}

FILES:${PN} += "${libdir}/python*"

INSANE_SKIP:${PN} += " already-stripped file-rdeps"

RDEPENDS:${PN} = " \
    ${PYTHON_PN}-numpy \
    ${PYTHON_PN}-protobuf \
    ${PYTHON_PN}-pip \
    libgomp \
"