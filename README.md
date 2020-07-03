# meta-st-stm32mpu-ai
OpenEmbedded meta layer to install AI frameworks and tools for the STM32MP1.
It also provide application samples.

## Compatibility
This version has been validated against the OpenSTLinux ecosystem release v1.2.0 and validated on STM32MP157x-DKx and STM32MP157x-EV1 boards.

## Available frameworks and tools within the meta-layer
[X-LINUX-AI v2.0.0 expansion package](https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package):
* TensorFlow Lite 2.2.0
* Coral Edge TPU accelerator support
* armNN 20.05
* OpenCV 4.1.x
* Python 3.8.x (enabling Pillow module)
* Support STM32MP15xF devices operating at up to 800MHz
* Python and C++ application samples
  * Image classification using TensorFlow Lite based on MobileNet v1 quantized model
  * Object detection using TensorFlow Lite based on COCO SSD MobileNet v1 quantized model
  * Image classification using Coral Edge TPU based on MobileNet v1 quantized model and compiled for the Coral Edge TPU
  * Object detection using Coral Edge TPU based on COCO SSD MobileNet v1 quantized model and compiled for the Coral Edge TPU
  * Image classification using armNN TensorFlow Lite parser based on MobileNet v1 float model
  * Object detection using armNN TensorFlow Lite parser based on COCO SSD MobileNet v1 quantized model

## Further information on how to install and how to use
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package>
<https://wiki.st.com/stm32mpu/wiki/How_to_install_X-LINUX-AI_v2.0.0_on_OpenSTLinux_v1.2.0>

## Application samples
<https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_application_samples_zoo>
