# meta-st-stm32mpu-app-ai-character-recognition-gui

OpenEmbedded meta layer to install ai character recognition UI based on Crank software solution.

## Installation of the meta layer

* Clone following git repositories into [your STM32MP1 Distribution path]/layers/meta-st
   > cd [your STM32MP1 Distribution path]/layers/meta-st <br>
   > git clone ssh://${USER}@gerrit.st.com:29418/stm32mpuapp/meta/meta-st-stm32mpu-ai.git -b thud <br>

* Setup the build environement
   > source layers/meta-st/scripts/envsetup.sh
   > * Select your DISTRO (ex: openstlinux-weston)
   > * Select your MACHINE (ex: stm32mp1)

* Add the new layers
   > bitbake-layers add-layer ../layers/meta-st/meta-st-stm32mpu-ai<br>

* Build your image
   > bitbake st-image-ai
