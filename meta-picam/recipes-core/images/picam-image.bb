SUMMARY = "Minimal Linux image for the picam person-detection camera"
DESCRIPTION = "Headless image: systemd, WiFi/Ethernet, SSH, OpenCV DNN person detector \
streaming MJPEG over HTTP. Nothing else."
LICENSE = "MIT"

inherit core-image

IMAGE_FEATURES += "ssh-server-dropbear"

IMAGE_INSTALL = " \
    packagegroup-core-boot \
    ${CORE_IMAGE_EXTRA_INSTALL} \
    picam-detector \
    network-config \
    avahi-daemon \
    systemd-analyze \
    v4l-utils \
"

IMAGE_LINGUAS = ""
