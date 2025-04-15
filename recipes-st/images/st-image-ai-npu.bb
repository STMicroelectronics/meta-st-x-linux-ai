require recipes-st/images/st-image-weston.bb

SUMMARY = "OpenSTLinux Artificial Inteligence for Computer Vision image based on weston image"

COMPATIBLE_MACHINE = "stm32mp2common"

# Define a proper userfs for st-image-ai
STM32MP_USERFS_IMAGE = "st-image-ai-npu-userfs"

# Define a proper vendorfs for st-image-ai
STM32MP_VENDORFS_IMAGE = "st-image-ai-npu-vendorfs"

# Define ROOTFS_MAXSIZE to 1.5GB
IMAGE_ROOTFS_MAXSIZE = "4194304"
# Define the size of userfs
STM32MP_USERFS_SIZE = "307200"
PARTITIONS_IMAGES[userfs]   = "${STM32MP_USERFS_IMAGE},${STM32MP_USERFS_LABEL},${STM32MP_USERFS_MOUNTPOINT},${STM32MP_USERFS_SIZE},FileSystem"

IMAGE_AI_PART = "   \
    packagegroup-x-linux-ai-demo-npu \
    apt-openstlinux-x-linux-ai-npu \
"

TOOLCHAIN_HOST_TASK:append = " nativesdk-x-linux-ai-tool "

#
# INSTALL addons
#
CORE_IMAGE_EXTRA_INSTALL += " \
    ${IMAGE_AI_PART}          \
"
