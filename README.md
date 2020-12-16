# meta-st-stm32mpu-ai
OpenEmbedded meta layer to install AI frameworks and tools for the STM32MP1.
It also provide application samples.

## Compatibility
This version has been validated against the OpenSTLinux ecosystem release v2.1.0, v2.0.0 and v1.2.0.
It supports STM32MP157x-DKx, STM32MP157x-EV1 and Avenger96 boards.

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v2.1.0 expansion package](https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package):
* TensorFlow Lite 2.4.1
* Coral Edge TPU accelerator support
  * libedgetpu 2.4.1 aligned with TensorFlow Lite 2.4.1 (built from source)
* armNN 20.11
* OpenCV 4.1.x
* Python 3.8.x (enabling Pillow module)
* Support STM32MP15xF devices operating at up to 800MHz
* Python and C++ application samples
  * Face recognition application using TensorFlow Lite capable of recognizion the face of a known (enrolled) user (Available on demand. Please contact the local STMicroelectronics support for more information about this application or send your request to [edge.ai@st.com](mailto:edge.ai@st.com)
  * Image classification application using TensorFlow Lite based on MobileNet v1 quantized model
  * Object detection application using TensorFlow Lite based on COCO SSD MobileNet v1 quantized model
  * Image classification application using Coral Edge TPU based on MobileNet v1 quantized model and compiled for the Coral Edge TPU
  * Object detection applicationusing Coral Edge TPU based on COCO SSD MobileNet v1 quantized model and compiled for the Coral Edge TPU
  * Image classification application using armNN TensorFlow Lite parser based on MobileNet v1 float model
  * Object detection application using armNN TensorFlow Lite parser based on COCO SSD MobileNet v1 quantized model

## Further information on how to install and how to use
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_application_samples_zoo>
