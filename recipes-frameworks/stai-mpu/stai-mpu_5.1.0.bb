DESCRIPTION = "STAI_MPU unified API is a software integration module provided by STMicroelectronics to facilitate \
deployment of Neural-Networks on all the STM32MPx product families through a unified API for all \
Hardwares accelerators and inference engines. It serves as the backend binding for runtime frameworks \
such as Tensorflow-Lite, Onnxruntime, OpenVX and more."
AUTHOR = "STMicroelectronics"
SUMMARY = "STAI_MPU unified API libraries and headers"
LICENSE = "SLA0044"
LIC_FILES_CHKSUM  = "file://LICENSE;md5=91fc08c2e8dfcd4229b69819ef52827c"

NO_GENERIC_LICENSE[SLA0044] = "LICENSE"
LICENSE:${PN} = "SLA0044"

SRC_URI  += " file://stai-mpu-api/"

S = "${WORKDIR}/stai-mpu-api"

python () {
    #Get major of the PV variable
    version = d.getVar('PV')
    version = version.split("+")
    version_base = version[0]
    version = version_base.split(".")
    major = version[0]
    python_version = d.getVar('PYTHON_BASEVERSION')
    python_version = python_version.replace(".", "")
    d.setVar('MAJOR', major)
    d.setVar('PVB', version_base)
    d.setVar('PYTHON_PV', python_version)
}

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp25common = "stm32mp2_npu"

inherit python3-dir

do_install() {
    # Install libdir
    install -d ${D}${libdir}
    # Include
    install -d ${D}${includedir}/stai_mpu
    # Install the Python package along with the bindings.
    mkdir -p ${D}${PYTHON_SITEPACKAGES_DIR}/stai_mpu
}

do_install:append:stm32mp1common(){
    # Install libdir
    install -m 0644 ${S}/aarch32/lib/libstai_mpu_ort.so.${MAJOR}    ${D}${libdir}/libstai_mpu_ort.so.${PVB}
    install -m 0644 ${S}/aarch32/lib/libstai_mpu_tflite.so.${MAJOR} ${D}${libdir}/libstai_mpu_tflite.so.${PVB}
    install -m 0644 ${S}/aarch32/lib/libstai_mpu.so.${MAJOR}        ${D}${libdir}/libstai_mpu.so.${PVB}

    ln -sf libstai_mpu.so.${PVB}        ${D}${libdir}/libstai_mpu.so.${MAJOR}
    ln -sf libstai_mpu.so.${PVB}        ${D}${libdir}/libstai_mpu.so
    ln -sf libstai_mpu_ort.so.${PVB}    ${D}${libdir}/libstai_mpu_ort.so.${MAJOR}
    ln -sf libstai_mpu_ort.so.${PVB}    ${D}${libdir}/libstai_mpu_ort.so
    ln -sf libstai_mpu_tflite.so.${PVB} ${D}${libdir}/libstai_mpu_tflite.so.${MAJOR}
    ln -sf libstai_mpu_tflite.so.${PVB} ${D}${libdir}/libstai_mpu_tflite.so

    # Include
    cp -r ${S}/aarch32/include/* ${D}${includedir}/stai_mpu

    # Install the Python package along with the bindings.
    cp -r ${S}/aarch32/python/*      ${D}${PYTHON_SITEPACKAGES_DIR}/stai_mpu/

    #Install specific python shared lib module for stm32mp1common
    cp ${S}/aarch32/lib/_stai_mpu_network.cpython.311-arm-linux-gnueabi-gnu.so  ${D}${PYTHON_SITEPACKAGES_DIR}/stai_mpu/_binding/_stai_mpu_network.so

    # Install STAI_MPU benchmark_model tool in binary and Python format
    install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    install -m 0755 ${S}/aarch32/tools/stai_mpu_benchmark         ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    install -m 0755 ${S}/aarch32/tools/stai_mpu_benchmark.py      ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/stai_mpu_benchmark
}

do_install:append:stm32mp25common(){
    # Install libdir
    install -m 0644 ${S}/aarch64/lib/libstai_mpu_ort.so.${MAJOR}    ${D}${libdir}/libstai_mpu_ort.so.${PVB}
    install -m 0644 ${S}/aarch64/lib/libstai_mpu_tflite.so.${MAJOR} ${D}${libdir}/libstai_mpu_tflite.so.${PVB}
    install -m 0644 ${S}/aarch64/lib/libstai_mpu.so.${MAJOR}        ${D}${libdir}/libstai_mpu.so.${PVB}

    ln -sf libstai_mpu.so.${PVB}        ${D}${libdir}/libstai_mpu.so.${MAJOR}
    ln -sf libstai_mpu.so.${PVB}        ${D}${libdir}/libstai_mpu.so
    ln -sf libstai_mpu_ort.so.${PVB}    ${D}${libdir}/libstai_mpu_ort.so.${MAJOR}
    ln -sf libstai_mpu_ort.so.${PVB}    ${D}${libdir}/libstai_mpu_ort.so
    ln -sf libstai_mpu_tflite.so.${PVB} ${D}${libdir}/libstai_mpu_tflite.so.${MAJOR}
    ln -sf libstai_mpu_tflite.so.${PVB} ${D}${libdir}/libstai_mpu_tflite.so

    # Include
    cp -r ${S}/aarch64/include/* ${D}${includedir}/stai_mpu

    # Install the Python package along with the bindings.
    cp -r ${S}/aarch64/python/*      ${D}${PYTHON_SITEPACKAGES_DIR}/stai_mpu/

    #Install specific python shared lib module for stm32mp25common
    cp ${S}/aarch64/lib/_stai_mpu_network.cpython.311-aarch64-linux-gnu.so      ${D}${PYTHON_SITEPACKAGES_DIR}/stai_mpu/_binding/_stai_mpu_network.so

    #Install specific OpenVX shared libs for stm32mp25common
    install -m 0644 ${S}/aarch64/lib/libstai_mpu_ovx.so.${MAJOR}    ${D}${libdir}/libstai_mpu_ovx.so.${PVB}
    ln -sf libstai_mpu_ovx.so.${PVB}    ${D}${libdir}/libstai_mpu_ovx.so.${MAJOR}
    ln -sf libstai_mpu_ovx.so.${PVB}    ${D}${libdir}/libstai_mpu_ovx.so

    # Install STAI_MPU benchmark_model tool in binary and Python format
    install -d ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    install -m 0755 ${S}/aarch64/tools/stai_mpu_benchmark         ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    install -m 0755 ${S}/aarch64/tools/stai_mpu_benchmark.py      ${D}${prefix}/local/bin/${PN}-${PVB}/tools
    chrpath -r '$ORIGIN' ${D}${prefix}/local/bin/${PN}-${PVB}/tools/stai_mpu_benchmark
}

PACKAGES += "${PN}-tools ${PYTHON_PN}-${PN} ${PN}-tflite ${PN}-ort "
PACKAGES:append:stm32mp25common = " ${PN}-ovx "
PROVIDES += "${PYTHON_PN}-${PN} ${PN}-tflite ${PN}-ort ${PN}-ovx"

FILES:${PN} = " ${libdir}/libstai_mpu.so.*  "
FILES:${PN}-tflite = "${libdir}/libstai_mpu_tflite.so.* "
FILES:${PN}-ort = " ${libdir}/libstai_mpu_ort.so.* "
FILES:${PN}-ovx = " ${libdir}/libstai_mpu_ovx.so.* "

FILES:${PN}-tools = "${prefix}/local/bin/${PN}-${PVB}/tools/*"
FILES:${PN}-dev = " ${libdir}/libstai_mpu.so \
                    ${libdir}/libstai_mpu_ovx.so \
                    ${libdir}/libstai_mpu_ort.so  \
                    ${libdir}/libstai_mpu_tflite.so \
                    ${includedir}/* "

INSANE_SKIP:${PN} += " dev-so already-stripped"

FILES:${PYTHON_PN}-${PN}  = "${PYTHON_SITEPACKAGES_DIR}/stai_mpu/*"

RDEPENDS:${PYTHON_PN}-${PN} += "${PYTHON_PN}-numpy \
                                ${PYTHON_PN}-typing-extensions \
                                ${PYTHON_PN}-pathlib2 "

RDEPENDS:${PN}-tools += "${PYTHON_PN}-${PN} \
                         ${PN} \
                         ${PYTHON_PN}-psutil \
                         ${PYTHON_PN}-core \
                         "

RDEPENDS:${PN}-tflite += " ${PYTHON_PN}-${PN} \
                           ${PN} \
                           ${PYTHON_PN}-tensorflow-lite \
                           tensorflow-lite \
                         "

RDEPENDS:${PN}-ort += "  ${PYTHON_PN}-${PN} \
                         ${PN} \
                         onnxruntime \
                         ${PYTHON_PN}-onnxruntime \
                         "

RDEPENDS:${PN}-ovx += "  ${PYTHON_PN}-${PN} \
                         ${PN} \
                         "

RDEPENDS:${PN}-ovx:append:stm32mp25common = " gcnano-driver-stm32mp \
                                              libopenvx-gcnano \
                                            "
