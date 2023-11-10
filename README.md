
<p align="center">
    <img width="720" src="https://raw.githubusercontent.com/STMicroelectronics/meta-st-stm32mpu-ai/master/x-linux-ai-logo.png">
</p>

X-LINUX-AI version: v5.0.0

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
The X-LINUX-AI OpenSTLinux Expansion Package v5.0.0 is compatible with the Yocto Project™ build system Mickledore.
It is validated over the OpenSTLinux Distribution v5.0 on STM32MP157F-DK2 with a USB image sensor, on STM32MP157F-EV1 with its built-in camera module, and on STM32MP135F-DK with its built-in camera module

## Versioning
Since its release v5.0.0, the major versioning of the X-LINUX-AI OpenSTLinux Expansion Package is aligned on the major versioning of the OpenSTLinux Distribution. This prevents painful backward compatibility attempts and makes dependencies straightforward.
The X-LINUX-AI generic versioning v**x**.**y**.**z** is built as follows:
* **x**: major version matching the OpenSTLinux Distribution major version. Each new major version is incompatible with previous OpenSTLinux Distribution versions.
* **y**: minor version, which is changed when new functionalities are added to the X-LINUX-AI OpenSTLinux Expansion Package in a backward compatible manner.
* **z**: patch version to introduce bug fixes. A patch version is implemented in a backward compatible manner.

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v5.0.0 expansion package](https://wiki.st.com/stm32mpu/wiki/Category:X-LINUX-AI_expansion_package):
* XNNPACK support for TensorFlow™ Lite and ONNX Runtime, with about 20% to 30% performance gain for quantized networks on a 32-bit system
* TensorFlow™ Lite 2.11.0 with XNNPACK delegate activated
* ONNX Runtime 1.14.0 with XNNPACK execution engine activated
* OpenCV 4.7.x
* Python™ 3.10.x (enabling Pillow module)
* Coral Edge TPU™ accelerator native support
  * libedgetpu 2.0.0 (Gouper) aligned with TensorFlow™ Lite 2.11.0
  * libcoral 2.0.0 (Gouper) aligned with TensorFlow™ Lite 2.11.0
  * PyCoral 2.0.0 (Gouper) aligned with TensorFlow™ Lite 2.11.0
* Support for the OpenSTLinux AI package repository allowing the installation of a prebuilt package using apt-* utilities
* Application samples
  * C++ / Python™ image classification example using TensorFlow™ Lite based on the MobileNet v1 quantized model
  * C++ / Python™ object detection example using TensorFlow™ Lite based on the COCO SSD MobileNet v1 quantized model
  * C++ / Python™ image classification example using Coral Edge TPU™ based on the MobileNet v1 quantized model and compiled for the Edge TPU™
  * C++ / Python™ object detection example using Coral Edge TPU™ based on the COCO SSD MobileNet v1 quantized model and compiled for the Edge TPU™
  * C++ face recognition application using proprietary model capable of recognizing the face of a known (enrolled) user. Contact the local STMicroelectronics support for more information about this application or send a request to edge.ai@st.com
  * Python™ image classification example using ONNX Runtime based on the MobileNet v1 quantized model
  * C++ / Python™ object detection example using ONNX Runtime based on the COCO SSD MobileNet v1 quantized model
* Application support for the 720p, 480p, and 272p display configurations
* X-LINUX-AI SDK add-on extending the OpenSTLinux SDK with AI functionality to develop and build an AI application easily. The X-LINUX-AI SDK add-on provides support for all the above frameworks. It is available from the [X-LINUX-AI](https://www.st.com/en/embedded-software/x-linux-ai.html) product page

## Further information on how to install and how to use X-LINUX-AI Starter package
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_Starter_package>

## Further information on how to install and how to use X-LINUX-AI Developer package
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_Developer_package>

## Further information on how to install and how to use X-LINUX-AI Distribution package
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_Distribution_package>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/Category:AI_-_Application_examples>
