# meta-st-stm32mpu-ai

OpenEmbedded meta layer to install ai frameworks for the STM32MP1 (NN,
Computer Vision, ...)

## Installation of the meta layer

* Clone following git repositories into [your STM32MP1 Distribution path]/layers/meta-st
   > cd [your STM32MP1 Distribution path]/layers/meta-st <br>
   > git clone https://gerrit.st.com/stm32mpuapp//meta/meta-st-stm32mpu-ai.git -b thud <br>

* Setup the build environement
   > source layers/meta-st/scripts/envsetup.sh
   > * Select your DISTRO (ex: openstlinux-weston)
   > * Select your MACHINE (ex: stm32mp1)

* Add the new layers
   > bitbake-layers add-layer ../layers/meta-st/meta-st-stm32mpu-ai<br>

* Build the AI image
   > bitbake st-image-ai

## Further information
https://wiki.st.com/stm32mpu/wiki/AI\_extension\_package
