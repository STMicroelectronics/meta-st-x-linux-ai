require recipes-st/images/st-image-userfs.bb

# Define a proper userfs for st-image-ai
STM32MP_USERFS_IMAGE = "st-image-ai-userfs"

# temporary fix
IMAGE_PARTITION_MOUNTPOINT = "/usr/local"

PACKAGE_INSTALL += "\
    packagegroup-x-linux-ai-demo \
"
