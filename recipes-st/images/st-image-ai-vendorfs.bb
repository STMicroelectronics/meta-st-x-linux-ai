require recipes-st/images/st-image-vendorfs.bb

# Define a proper vendorfs for st-image-ai
STM32MP_VENDORFS_IMAGE = "st-image-ai-vendorfs"

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp2common = "${@bb.utils.contains('MACHINE_FEATURES', 'gpu', 'stm32mp2_npu', 'stm32mp2', d)}"
BOARD_USED:stm32mp2common := "${@bb.utils.contains('DEFAULT_BUILD_AI', 'CPU', 'stm32mp2', '${BOARD_USED}', d)}"

# add vendorfs packages to support NPU
PACKAGE_INSTALL += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', ' libopenvx-gcnano libovxkernels-gcnano', '', d)}"
STM32MP_VENDORFS_SIZE:stm32mp2common = "215040"
