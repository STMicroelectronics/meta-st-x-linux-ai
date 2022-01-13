<p align="center">
    <img width="720" src="https://raw.githubusercontent.com/STMicroelectronics/meta-st-stm32mpu-ai/master/x-linux-ai-logo.png">
</p>

# meta-st-stm32mpu-ai
OpenEmbedded meta layer to install AI frameworks and tools for the STM32MP1.
It also provide application samples.

## Compatibility
This version has been validated against the OpenSTLinux ecosystem release v2.1.0, v3.0.1.
It supports STM32MP157x-DKx, STM32MP157x-EV1 and Avenger96 boards.
Compatible with Yoto Project build system Dunfell.

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v2.1.0 expansion package](https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package):
* TensorFlow Lite 2.5.0
* armNN 21.05
* OpenCV 4.1.x
* Python 3.8.x (enabling Pillow module)
* Support STM32MP15xF devices operating at up to 800MHz
* Support of the Avenger96 board from Linaro™ 96Boards based on the STM32MP157A microprocessor, either with a USB camera or the DesignCore® OV5640 camera mezzanine board from D3 Engineering tested with the OpenSTLinux Distribution v2.1.0
* Coral Edge TPU accelerator support
  * libedgetpu 2.5.0 (built from master branch source code) and aligned with TensorFlow Lite 2.5.0
* The X-LINUX-AI OpenSTLinux Expansion Package v2.1.1 is compatible with Yocto Project® build systems Dunfell. As a consequence, it is compatible with OpenSTLinux Distributions v2.x and v3.x on STM32MP157C-DK2 with a USB camera, and on STM32MP157A-EV1 and STM32MP157C-EV1 with their built-in camera module
* Support for the OpenSTLinux AI package repository allowing the installation of prebuilt package using apt-* utilities
* Application samples
  * C++ / Python™ image classification application using TensorFlow™ Lite based on MobileNet v1 quantized model
  * C++ / Python™ object detection application using TensorFlow™ Lite based on COCO SSD MobileNet v1 quantized model
  * C++ / Python™ image classification application using Coral Edge TPU™ based on MobileNet v1 quantized model and compiled for the Coral Edge TPU
  * C++ / Python™ object detection applicationusing Coral Edge TPU™ based on COCO SSD MobileNet v1 quantized model and compiled for the Coral Edge TPU
  * C++ image classification application using armNN TensorFlow™ Lite parser based on MobileNet v1 float model
  * C++ object detection application using armNN TensorFlow™ Lite parser based on COCO SSD MobileNet v1 quantized model
  * C++ face recognition application using proprietary model capable of recognizing the face of a known (enrolled) user. Contact the local STMicroelectronics support for more information about this application or send a request to [edge.ai@st.com](mailto:edge.ai@st.com)
* Application support 720p, 480p and 272p display configurations
* Application user interface with updated look and feel

## Further information on how to install and how to use
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_application_samples_zoo>
