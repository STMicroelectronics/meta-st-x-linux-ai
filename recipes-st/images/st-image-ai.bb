require recipes-st/images/st-image-weston.bb

SUMMARY = "OpenSTLinux Artificial Inteligence for Computer Vision image based on weston image"

# Define ROOTFS_MAXSIZE to 4GB
IMAGE_ROOTFS_MAXSIZE = "4194304"
# Define the size of userfs
STM32MP_USERFS_SIZE = "307200"
# Define the size of vendorfs
STM32MP_VENDORFS_SIZE:stm32mp2common = "215040"

PARTITIONS_IMAGES[userfs]   = "${STM32MP_USERFS_IMAGE},${STM32MP_USERFS_LABEL},${STM32MP_USERFS_MOUNTPOINT},${STM32MP_USERFS_SIZE},FileSystem"

BOARD_USED:stm32mp1common = "stm32mp1"
BOARD_USED:stm32mp2common = "${@bb.utils.contains('MACHINE_FEATURES', 'gpu', 'stm32mp2_npu', 'stm32mp2', d)}"
BOARD_USED:stm32mp2common := "${@bb.utils.contains('DEFAULT_BUILD_AI', 'CPU', 'stm32mp2', '${BOARD_USED}', d)}"

IMAGE_AI_NPU_PART = "   \
    packagegroup-x-linux-ai-demo-npu \
    apt-openstlinux-x-linux-ai-npu \
"

IMAGE_AI_CPU_PART = "   \
    packagegroup-x-linux-ai-demo-cpu \
    apt-openstlinux-x-linux-ai-cpu \
"

TOOLCHAIN_HOST_TASK:append = " nativesdk-x-linux-ai-tool "

#
# INSTALL addons
#
PACKAGE_INSTALL += " ${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', ' libopenvx-gcnano libovxkernels-gcnano', '', d)}"
CORE_IMAGE_EXTRA_INSTALL += "${@bb.utils.contains('BOARD_USED', 'stm32mp2_npu', '${IMAGE_AI_NPU_PART}', '${IMAGE_AI_CPU_PART}', d)}"
