require recipes-st/images/st-image-userfs.bb

# Define a proper userfs for st-image-ai
STM32MP_USERFS_IMAGE = "st-image-ai-userfs"

# temporary fix
IMAGE_PARTITION_MOUNTPOINT = "/usr/local"

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp2common = "${@bb.utils.contains('MACHINE_FEATURES', 'gpu', 'stm32mp2_npu', 'stm32mp2', d)}"
BOARD_USED:stm32mp2common := "${@bb.utils.contains('DEFAULT_BUILD_AI', 'CPU', 'stm32mp2', '${BOARD_USED}', d)}"

AI_NPU_PACKAGE += "\
    packagegroup-x-linux-ai-demo-npu \
    apt-openstlinux-x-linux-ai-npu \
"

AI_CPU_PACKAGE += "\
    packagegroup-x-linux-ai-demo-cpu \
    apt-openstlinux-x-linux-ai-cpu \
"

PACKAGE_INSTALL += "${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${AI_NPU_PACKAGE}', '${AI_CPU_PACKAGE}', d)}"