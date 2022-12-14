<p align="center">
    <img width="720" src="https://raw.githubusercontent.com/STMicroelectronics/meta-st-stm32mpu-ai/master/x-linux-ai-logo.png">
</p>

X-LINUX-AI is a free of charge open-source software package dedicated to AI.
It is a complete ecosystem that allow developers working with OpenSTLinux to create AI-based application very easily.
* **All-in-one AI solutions** for the entire STM32MPU serie
* **Pre-integrated** into Linux distribution based on ST environment
* Include **AI frameworks** to execute Neural Network models
* Include **AI model benchmark** application tools for MPU
* **Easy** application **prototyping** using Python language and AI frameworks Python API
* **C++ API** for embedded high-performance applications
* Optimized **open-source solutions** provided with source codes that allow extensive **code reuse** and **time savings**


# meta-st-stm32mpu-ai
X-LINUX-AI OpenEmbedded meta layer to be integrated into OpenSTLinux distribution.
It contains recipes for AI frameworks, tools and application examples for STM32MPx series

## Compatibility
The X-LINUX-AI OpenSTLinux Expansion Package v3.0.0 is compatible with the Yocto Project™ build systems Kirkston.
It is validated over the OpenSTLinux Distributions v4.1 on STM32MP157F-DK2 with a USB image sensor and on STM32MP157F-EV1 with its built-in camera module

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v3.0.0 expansion package](https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package):
* TensorFlow™ Lite 2.11.0
* ONNX Runtime 1.11.0
* OpenCV 4.5.x
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
* Application support for the 720p, 480p, and 272p display configurations
* X-LINUX-AI SDK add-on extending the OpenSTLinux SDK with AI functionality to develop and build an AI application easily. The X-LINUX-AI SDK add-on provides support for all the above frameworks. It is available from the [X-LINUX-AI](https://www.st.com/en/embedded-software/x-linux-ai.html) product page

## Further information on how to install and how to use X-LINUX-AI
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package>

## Further information on how to install and how to use X-LINUX-AI SDK add-on
<https://wiki.st.com/stm32mpu/wiki/How_to_install_and_use_the_X-LINUX-AI_SDK_add-on>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_application_samples_zoo>
