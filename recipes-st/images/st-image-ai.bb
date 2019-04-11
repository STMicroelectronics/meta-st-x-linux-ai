require ../../../meta-st-openstlinux/recipes-st/images/st-image-weston.bb

SUMMARY = "OpenSTLinux Artificial Inteligence image based on weston image"

IMAGE_AI_PART = "   \
    opencv \
"

#
# INSTALL addons
#
CORE_IMAGE_EXTRA_INSTALL += " \
    ${IMAGE_AI_PART}          \
"
