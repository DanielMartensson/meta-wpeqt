SUMMARY = "Minimal STM32MP image with the WpeQt browser"
DESCRIPTION = "Small headless-friendly image that boots into a Wayland/SDL \
session and starts WpeQt once the compositor is up."
LICENSE = "MIT"

inherit core-image-base

IMAGE_INSTALL:append = " \
    packagegroup-wpeqt \
"