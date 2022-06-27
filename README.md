<p align="center">
    <img width="720" src="https://raw.githubusercontent.com/STMicroelectronics/meta-st-stm32mpu-ai/master/x-linux-ai-logo.png">
</p>

# meta-st-stm32mpu-ai
OpenEmbedded meta layer to install AI frameworks and tools for the STM32MP1.
It also provide application samples.

## Compatibility
The X-LINUX-AI OpenSTLinux Expansion Package v2.2.0 is compatible with the Yocto Project™ build systems Kirkstone and Dunfell.
It is validated over the OpenSTLinux Distributions v3.1 and v4.0 on STM32MP157C-DK2 with a USB image sensor, and on STM32MP157A-EV1 and STM32MP157C-EV1 with their built-in camera module

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v2.2.0 expansion package](https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package):
* TensorFlow™ Lite 2.8.0
* OpenCV 4.5.x
* Python™ 3.10.x (enabling Pillow module)
* Support for the STM32MP157F devices operating at up to 800 MHz
* Coral Edge TPU™ accelerator native support
  * libedgetpu 2.0.0 (Gouper) aligned with TensorFlow™ Lite 2.8.0
  * libcoral 2.0.0 (Gouper) aligned with TensorFlow™ Lite 2.8.0
  * PyCoral 2.0.0 (Gouper) aligned with TensorFlow™ Lite 2.8.0
* Support for the OpenSTLinux AI package repository allowing the installation of a prebuilt package using apt-* utilities
* Application samples
  * C++ / Python™ image classification example using TensorFlow™ Lite based on the MobileNet v1 quantized model
  * C++ / Python™ object detection example using TensorFlow™ Lite based on the COCO SSD MobileNet v1 quantized model
  * C++ / Python™ image classification example using Coral Edge TPU™ based on the MobileNet v1 quantized model and compiled for the Edge TPU™
  * C++ / Python™ object detection example using Coral Edge TPU™ based on the COCO SSD MobileNet v1 quantized model and compiled for the Edge TPU™
  * C++ face recognition application using proprietary model capable of recognizing the face of a known (enrolled) user. Contact the local STMicroelectronics support for more information about this application or send a request to edge.ai@st.com
* Application support for the 720p, 480p, and 272p display configurations
* Application user interface with updated look and feel
* Python™ and C++ application rework for better performance
* X-LINUX-AI SDK add-on extending the OpenSTLinux SDK with AI functionality to develop and build an AI application easily. The X-LINUX-AI SDK add-on provides support for all the above frameworks. It is available from the [X-LINUX-AI](https://www.st.com/en/embedded-software/x-linux-ai.html) product page

## Further information on how to install and how to use X-LINUX-AI
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package>

## Further information on how to install and how to use X-LINUX-AI SDK add-on
<https://wiki.st.com/stm32mpu/wiki/How_to_install_and_use_the_X-LINUX-AI_SDK_add-on>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_application_samples_zoo>
