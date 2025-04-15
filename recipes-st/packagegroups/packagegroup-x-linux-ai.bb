SUMMARY = "X-LINUX-AI full components (frameworks and application samples)"

PACKAGE_ARCH = "${MACHINE_ARCH}"

inherit packagegroup python3-dir

PROVIDES = "${PACKAGES}"
PACKAGES = "                                 \
    packagegroup-x-linux-ai-cpu              \
    packagegroup-x-linux-ai-demo-cpu         \
    packagegroup-x-linux-ai-tflite-cpu       \
    packagegroup-x-linux-ai-onnxruntime-cpu  \
"
PACKAGES:append:stm32mp2common = " packagegroup-x-linux-ai packagegroup-x-linux-ai-npu packagegroup-x-linux-ai-demo-npu packagegroup-x-linux-ai-tflite-npu packagegroup-x-linux-ai-onnxruntime-npu "

# Manage to provide only demo with best performances depending on target used
RDEPENDS:packagegroup-x-linux-ai-demo-cpu = " packagegroup-x-linux-ai-tflite-cpu "
RDEPENDS:packagegroup-x-linux-ai-demo-npu:append:stm32mp2common = " packagegroup-x-linux-ai-npu "

# Manage to provide all framework tools base packages with overall one
RDEPENDS:packagegroup-x-linux-ai-cpu = "     \
    packagegroup-x-linux-ai-tflite-cpu       \
    packagegroup-x-linux-ai-onnxruntime-cpu  \
"

# Manage to provide all framework tools base packages with overall one
RDEPENDS:packagegroup-x-linux-ai:append:stm32mp2common = "     \
    packagegroup-x-linux-ai-tflite-npu       \
    packagegroup-x-linux-ai-onnxruntime-npu  \
    packagegroup-x-linux-ai-npu \
"

SUMMARY:packagegroup-x-linux-ai-tflite-cpu = "X-LINUX-AI TensorFlow Lite components (CPU only)"
RDEPENDS:packagegroup-x-linux-ai-tflite-cpu = "  \
    stai-mpu-tflite                          \
    stai-mpu-tools                           \
    ${PYTHON_PN}-stai-mpu                    \
    ${PYTHON_PN}-tensorflow-lite             \
    tensorflow-lite-tools                    \
    tensorflow-lite                          \
    x-linux-ai-tool                          \
    x-linux-ai-application                   \
    x-linux-ai-benchmark                     \
    stai-mpu-image-classification-cpp-tfl-cpu    \
    stai-mpu-image-classification-python-tfl-cpu \
    stai-mpu-object-detection-cpp-tfl-cpu        \
    stai-mpu-object-detection-python-tfl-cpu     \
"

SUMMARY:packagegroup-x-linux-ai-tflite-npu = "X-LINUX-AI TensorFlow Lite components (CPU/GPU/NPU)"
RDEPENDS:packagegroup-x-linux-ai-tflite-npu:append:stm32mp2common = "  \
    stai-mpu-tflite                          \
    stai-mpu-tools                           \
    ${PYTHON_PN}-stai-mpu                    \
    ${PYTHON_PN}-tensorflow-lite             \
    tensorflow-lite-tools                    \
    tensorflow-lite                          \
    x-linux-ai-tool                          \
    x-linux-ai-application                   \
    x-linux-ai-benchmark                     \
    stai-mpu-image-classification-cpp-tfl-npu    \
    stai-mpu-image-classification-python-tfl-npu \
    stai-mpu-object-detection-cpp-tfl-npu        \
    stai-mpu-object-detection-python-tfl-npu     \
    tflite-vx-delegate                       \
    tflite-vx-delegate-example               \
    tim-vx-tools                             \
"

SUMMARY:packagegroup-x-linux-ai-onnxruntime-cpu = "X-LINUX-AI ONNX Runtime components (CPU only)"
RDEPENDS:packagegroup-x-linux-ai-onnxruntime-cpu = " \
    stai-mpu-ort                             \
    stai-mpu-tools                           \
    ${PYTHON_PN}-stai-mpu                    \
    onnxruntime                              \
    onnxruntime-tools                        \
    ${PYTHON_PN}-onnxruntime                 \
    x-linux-ai-tool                          \
    x-linux-ai-benchmark                     \
    x-linux-ai-application                   \
    stai-mpu-image-classification-cpp-ort-cpu    \
    stai-mpu-image-classification-python-ort-cpu \
    stai-mpu-object-detection-python-ort-cpu     \
    stai-mpu-object-detection-cpp-ort-cpu        \
"

SUMMARY:packagegroup-x-linux-ai-onnxruntime-npu = "X-LINUX-AI ONNX Runtime components (CPU/GPU/NPU)"
RDEPENDS:packagegroup-x-linux-ai-onnxruntime-npu:append:stm32mp2common = " \
    stai-mpu-ort                             \
    stai-mpu-tools                           \
    ${PYTHON_PN}-stai-mpu                    \
    onnxruntime                              \
    onnxruntime-tools                        \
    ${PYTHON_PN}-onnxruntime                 \
    x-linux-ai-tool                          \
    x-linux-ai-benchmark                     \
    x-linux-ai-application                   \
    stai-mpu-image-classification-cpp-ort-npu    \
    stai-mpu-image-classification-python-ort-npu \
    stai-mpu-object-detection-python-ort-npu     \
    stai-mpu-object-detection-cpp-ort-npu        \
    ort-vsinpu-ep-example-cpp                    \
    ort-vsinpu-ep-example-python                 \
    tim-vx-tools                                 \
"

SUMMARY:packagegroup-x-linux-ai-npu = "X-LINUX-AI minimum NPU components"
RDEPENDS:packagegroup-x-linux-ai-npu:append:stm32mp2common = " \
    stai-mpu-ovx                  \
    stai-mpu-tools                \
    ${PYTHON_PN}-stai-mpu         \
    tim-vx                        \
    tim-vx-tools                  \
    nbg-benchmark                 \
    x-linux-ai-tool               \
    x-linux-ai-benchmark          \
    x-linux-ai-application        \
    tflite-vx-delegate-example    \
    ort-vsinpu-ep-example-cpp     \
    ort-vsinpu-ep-example-python  \
    stai-mpu-image-classification-cpp-ovx-npu     \
    stai-mpu-image-classification-python-ovx-npu  \
    stai-mpu-object-detection-cpp-ovx-npu         \
    stai-mpu-object-detection-python-ovx-npu      \
    stai-mpu-semantic-segmentation-python-ovx-npu \
    stai-mpu-pose-estimation-python-ovx-npu       \
    stai-mpu-face-recognition-cpp-ovx-npu         \
"

RCONFLICTS:packagegroup-x-linux-ai-onnxruntime-npu = "packagegroup-x-linux-ai-onnxruntime-cpu"
RCONFLICTS:packagegroup-x-linux-ai-onnxruntime-cpu = "packagegroup-x-linux-ai-onnxruntime-npu"
RCONFLICTS:packagegroup-x-linux-ai-tflite-npu = "packagegroup-x-linux-ai-tflite-cpu"
RCONFLICTS:packagegroup-x-linux-ai-tflite-cpu = "packagegroup-x-linux-ai-tflite-npu"
RCONFLICTS:packagegroup-x-linux-ai-demo-npu = "packagegroup-x-linux-ai-demo-cpu"
RCONFLICTS:packagegroup-x-linux-ai-demo-cpu = "packagegroup-x-linux-ai-demo-npu"
RCONFLICTS:packagegroup-x-linux-ai = "packagegroup-x-linux-ai-cpu"
RCONFLICTS:packagegroup-x-linux-ai-cpu = "packagegroup-x-linux-ai"