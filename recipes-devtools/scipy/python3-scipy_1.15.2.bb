DESCRIPTION = "SciPy: Scientific Library for Python"
HOMEPAGE = "https://www.scipy.org/"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=a0cba8e04f4e1e269ab845f671de85b7"

SRC_URI = "https://files.pythonhosted.org/packages/f0/24/1a181a9e5050090e0b5138c5f496fee33293c342b788d02586bc410c6477/scipy-1.15.2-cp312-cp312-manylinux_2_17_aarch64.manylinux2014_aarch64.whl"
SRC_URI[md5sum] = "0911ce926916b1fdb0e0c51a947e9be3"
SRC_URI[sha256sum] = "5a6fd6eac1ce74a9f77a7fc724080d507c5812d61e72bd5e4c489b042455865e"
SRC_URI += " file://LICENSE.txt "

inherit python3-dir

S = "${WORKDIR}"

DEPENDS += " \
        unzip-native \
    "

do_install() {
    mkdir -p ${S}/unpacked
    unzip -o ${S}/$(basename ${SRC_URI}) -d ${S}/unpacked
    install -d ${D}${PYTHON_SITEPACKAGES_DIR}/
    install -d ${D}${libdir}/

    install -m 0644 ${S}/unpacked/scipy.libs/libgfortran-daac5196.so.5.0.0 ${D}${libdir}/
    install -m 0644 ${S}/unpacked/scipy.libs/libgfortran-daac5196-038a5e3c.so.5.0.0 ${D}${libdir}/
    install -m 0644 ${S}/unpacked/scipy.libs/libscipy_openblas-9778f98e.so ${D}${libdir}/

    cp -r  ${S}/unpacked/scipy ${D}${PYTHON_SITEPACKAGES_DIR}/
    cp -r  ${S}/unpacked/scipy-1.15.2.dist-info ${D}${PYTHON_SITEPACKAGES_DIR}/
}

FILES:${PN} += "${PYTHON_SITEPACKAGES_DIR}/ ${libdir}/ "
INSANE_SKIP:${PN}-dev += "already-stripped dev-deps dev-elf file-rdeps"
INSANE_SKIP:${PN} += "already-stripped dev-deps dev-elf file-rdeps"