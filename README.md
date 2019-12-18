# meta-st-stm32mpu-ai
OpenEmbedded meta layer to install AI frameworks and tools for the STM32MP1.

Available images:
* st-image-ai-cv for Computer Vision

## Compatibility
Compatible with the following OpenSTLinux Distribution release v1.1.0:
* openstlinux-4.19-thud-mp1-19-10-09

## Available frameworks and tools within the meta-layer
[X-LINUX-AI-CV v1.1.0 expansion package](https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI-CV_OpenSTLinux_expansion_package):
* TensorFlow Lite 2.0.0
* OpenCV 3.4.x
* Python 3.5.x (enabling Pillow module)
* Python application examples
  * Image classification example based on MobileNet v1 model
  * Object detection example based on COCO SSD MobileNet v1 model
* C/C++ application examples
  * Image classification example based on MobileNet v1 model
  * Object detection example based on COCO SSD MobileNet v1 model
* Support of the STM32MP157 Avenger96 board + OV5640 CSI camera mezzanine board

## Further information (how to install, how to use, ...)
<https://wiki.st.com/stm32mpu/wiki/STM32MP1_artificial_intelligence_expansion_packages>
