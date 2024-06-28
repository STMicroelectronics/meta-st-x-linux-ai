<p align="center">
    <img width="720" src="https://raw.githubusercontent.com/STMicroelectronics/meta-st-stm32mpu-ai/master/x-linux-ai-logo.png">
</p>

X-LINUX-AI version: v5.1.0

X-LINUX-AI is a free of charge open-source software package dedicated to AI.
It is a complete ecosystem that allow developers working with OpenSTLinux to create AI-based application very easily.
* **All-in-one AI solutions** for the entire STM32MPU serie
* **Pre-integrated** into Linux distribution based on ST environment
* Include **AI frameworks** to execute Neural Network models
* Include **AI model benchmark** application tools for MPU
* **Easy** application **prototyping** using Python language and AI frameworks Python API
* **C++ API** for embedded high-performance applications
* Optimized **open-source solutions** provided with source codes that allow extensive **code reuse** and **time savings**

# meta-st-x-linux-ai
X-LINUX-AI OpenEmbedded meta layer to be integrated into OpenSTLinux distribution.
It contains recipes for AI frameworks, tools and application examples for STM32MPx series

## Compatibility
The X-LINUX-AI OpenSTLinux Expansion Package v5.1.0 is compatible with the Yocto Project™ build system Mickledore.
It is validated over the OpenSTLinux Distribution v5.1.0 on STM32MP25x and STM32MP1x series.

## Versioning
Since its release v5.0.0, the major versioning of the X-LINUX-AI OpenSTLinux Expansion Package is aligned on the major versioning of the OpenSTLinux Distribution. This prevents painful backward compatibility attempts and makes dependencies straightforward.
The X-LINUX-AI generic versioning v**x**.**y**.**z** is built as follows:
* **x**: major version matching the OpenSTLinux Distribution major version. Each new major version is incompatible with previous OpenSTLinux Distribution versions.
* **y**: minor version, which is changed when new functionalities are added to the X-LINUX-AI OpenSTLinux Expansion Package in a backward compatible manner.
* **z**: patch version to introduce bug fixes. A patch version is implemented in a backward compatible manner.

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v5.1.0 expansion package](https://wiki.st.com/stm32mpu/wiki/Category:X-LINUX-AI_expansion_package):
* AI Frameworks:
  * STAI_MPU Unified API based on OpenVX™(STM32MP25x only), TensorFlow™ Lite, Coral Edge TPU™ and ONNX Runtime™ compatible with all STM32MPU series
  * TIM-VX™ 1.1.57 (STM32MP25x only)
  * TensorFlow™ Lite 2.11.0 (CPU only) with XNNPACK delegate activated
  * ONNX Runtime™ 1.14.0 (CPU only) with XNNPACK execution engine activated
  * Coral Edge TPU™ accelerator native support
    * libedgetpu 2.0.0 (Grouper) aligned with TensorFlow™ Lite 2.11.0
    * libcoral 2.0.0 (Grouper) aligned with TensorFlow™ Lite 2.11.0
    * PyCoral 2.0.0 (Grouper) aligned with TensorFlow™ Lite 2.11.0

* Out of the box applications:
  * Image classification :
    * C++ / Python™ example using STAI_MPU Unified API]] based on the MobileNet v1 and v2 quantized models
  * Object detection :
    * C++ / Python™ example using STAI_MPU Unified API]] based on the SSD MobileNet v1 and v2 quantized models
    * Python™ example using STAI_MPU Unified API]] based on YoloV8n pose quantized model
  * Semantic segmentation :
    * Python™ example using STAI_MPU Unified API]] based on DeepLabV3 quantized model
  * Face recognition:
    * C++ example using proprietary model capable of recognizing the face of a known (enrolled) user.
      * Contact the local STMicroelectronics support for more information about this application or send a request to edge.ai@st.com
  * Note: applications are based on Gstreamer 1.22.x, GTK 3.x, OpenCV 4.7.x, Pillow, Python 3

* Utilities:
  * X-LINUX-AI tool suite provides tools for software information, AI packages management and Neural Network models benchmarking.
  * Support wide range of image sensors for ST MPU including IMX335 (5MP) for MP2 with use of internal ISP, GC2145 and OV5640 for STM32MP13x
  * Support for the OpenSTLinux AI package repository allowing the installation of a prebuilt package using apt-*

* Host tools:
  * ST Edge AI tool for NBG generation
  * X-LINUX-AI SDK add-on extending the OpenSTLinux SDK with AI functionality to develop and build an AI application easily. The X-LINUX-AI SDK add-on supports all the above frameworks. It is available from the X-LINUX-AI product page

## Further information on how to install and how to use X-LINUX-AI Starter package
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_Starter_package>

## Further information on how to install and how to use X-LINUX-AI Developer package
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_Developer_package>

## Further information on how to install and how to use X-LINUX-AI Distribution package
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_Distribution_package>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/Category:AI_-_Application_examples>
