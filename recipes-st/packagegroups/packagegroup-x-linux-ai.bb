SUMMARY = "X-LINUX-AI full components (frameworks and application samples)"

PACKAGE_ARCH = "${MACHINE_ARCH}"

inherit packagegroup python3-dir

PROVIDES = "${PACKAGES}"
PACKAGES = "                                 \
    packagegroup-x-linux-ai                  \
    packagegroup-x-linux-ai-demo             \
    packagegroup-x-linux-ai-tflite           \
    packagegroup-x-linux-ai-onnxruntime      \
"
PACKAGES:append:stm32mp25common = " packagegroup-x-linux-ai-npu"

# Manage to provide only demo with best performances depending on target used
RDEPENDS:packagegroup-x-linux-ai-demo:append:stm32mp1common = " packagegroup-x-linux-ai-tflite "
RDEPENDS:packagegroup-x-linux-ai-demo:append:stm32mp25common = " packagegroup-x-linux-ai-npu "

# Manage to provide all framework tools base packages with overall one
RDEPENDS:packagegroup-x-linux-ai = "         \
    packagegroup-x-linux-ai-tflite           \
    packagegroup-x-linux-ai-onnxruntime      \
"
RDEPENDS:packagegroup-x-linux-ai:append:stm32mp25common = " packagegroup-x-linux-ai-npu"

SUMMARY:packagegroup-x-linux-ai-tflite = "X-LINUX-AI TensorFlow Lite components"
RDEPENDS:packagegroup-x-linux-ai-tflite = "  \
    stai-mpu-tflite                          \
    stai-mpu-tools                           \
    ${PYTHON_PN}-stai-mpu                    \
    ${PYTHON_PN}-tensorflow-lite             \
    tensorflow-lite-tools                    \
    tensorflow-lite                          \
    x-linux-ai-tool                          \
    x-linux-ai-benchmark                     \
    stai-mpu-image-classification-cpp-tfl    \
    stai-mpu-image-classification-python-tfl \
    stai-mpu-object-detection-cpp-tfl        \
    stai-mpu-object-detection-python-tfl     \
"

SUMMARY:packagegroup-x-linux-ai-onnxruntime = "X-LINUX-AI ONNX Runtime components"
RDEPENDS:packagegroup-x-linux-ai-onnxruntime = " \
    stai-mpu-ort                             \
    stai-mpu-tools                           \
    ${PYTHON_PN}-stai-mpu                    \
    onnxruntime                              \
    onnxruntime-tools                        \
    ${PYTHON_PN}-onnxruntime                 \
    x-linux-ai-tool                          \
    x-linux-ai-benchmark                     \
    stai-mpu-image-classification-cpp-ort    \
    stai-mpu-image-classification-python-ort \
    stai-mpu-object-detection-python-ort     \
    stai-mpu-object-detection-cpp-ort        \
"

SUMMARY:packagegroup-x-linux-ai-npu = "X-LINUX-AI minimum NPU components"
RDEPENDS:packagegroup-x-linux-ai-npu += "\
    stai-mpu-ovx              \
    stai-mpu-tools            \
    ${PYTHON_PN}-stai-mpu     \
    tim-vx                    \
    tim-vx-tools              \
    nbg-benchmark             \
    x-linux-ai-tool           \
    x-linux-ai-benchmark      \
"

RDEPENDS:packagegroup-x-linux-ai-npu:append:stm32mp25common = " stai-mpu-image-classification-cpp-ovx     \
                                                                stai-mpu-image-classification-python-ovx  \
                                                                stai-mpu-object-detection-cpp-ovx \
                                                                stai-mpu-object-detection-python-ovx \
                                                                stai-mpu-semantic-segmentation-python-ovx \
                                                                stai-mpu-pose-estimation-python-ovx \
                                                                stai-mpu-face-recognition-cpp-ovx         \
"