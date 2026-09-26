SUMMARY = "Minimal Linux image for the picam person-detection camera"
DESCRIPTION = "Headless image: systemd, WiFi/Ethernet, SSH, OpenCV DNN person detector \
streaming MJPEG over HTTP. Nothing else."
LICENSE = "MIT"

inherit core-image

IMAGE_FEATURES += "ssh-server-dropbear"

# MACHINE_EXTRA_RRECOMMENDS: module kernel (brcmfmac, uvcvideo) + firmware WiFi của
# từng board. Bình thường packagegroup-base kéo vào; image tối giản này thì phải ghi rõ,
# thiếu nó Pi sẽ không vào được WiFi.
IMAGE_INSTALL = " \
    packagegroup-core-boot \
    ${MACHINE_EXTRA_RRECOMMENDS} \
    ${CORE_IMAGE_EXTRA_INSTALL} \
    picam-detector \
    network-config \
    avahi-daemon \
    systemd-analyze \
    v4l-utils \
"

IMAGE_LINGUAS = ""
