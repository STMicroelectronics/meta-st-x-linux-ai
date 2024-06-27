require recipes-st/images/st-image-vendorfs.bb

# Define a proper vendorfs for st-image-ai
STM32MP_VENDORFS_IMAGE = "st-image-ai-vendorfs"

# add vendorfs packages to support NPU
PACKAGE_INSTALL:append:stm32mp25common = " libopenvx-gcnano libovxkernels-gcnano "
STM32MP_VENDORFS_SIZE:stm32mp25common = "157696"
