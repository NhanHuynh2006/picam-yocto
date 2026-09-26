SUMMARY = "Realtime person-detection camera: V4L2 capture, OpenCV DNN, MJPEG over HTTP"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://picam-detector \
    file://picam-detector.service \
    file://picam.conf.in \
"
S = "${WORKDIR}/picam-detector"

DEPENDS = "opencv"

inherit cmake systemd

# Giá trị mặc định; được ghi đè theo board trong kas/<board>.yml
PICAM_CAM_WIDTH ?= "640"
PICAM_CAM_HEIGHT ?= "480"
PICAM_INPUT_SIZE ?= "320"
PICAM_STREAM_FPS ?= "8"

SYSTEMD_SERVICE:${PN} = "picam-detector.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/picam-detector.service ${D}${systemd_system_unitdir}/

    install -d ${D}${sysconfdir}/default
    sed -e 's|@CAM_WIDTH@|${PICAM_CAM_WIDTH}|' \
        -e 's|@CAM_HEIGHT@|${PICAM_CAM_HEIGHT}|' \
        -e 's|@INPUT_SIZE@|${PICAM_INPUT_SIZE}|' \
        -e 's|@STREAM_FPS@|${PICAM_STREAM_FPS}|' \
        ${WORKDIR}/picam.conf.in > ${D}${sysconfdir}/default/picam
    chmod 0644 ${D}${sysconfdir}/default/picam
}

CONFFILES:${PN} = "${sysconfdir}/default/picam"

RDEPENDS:${PN} = "picam-model"
RRECOMMENDS:${PN} = "kernel-module-uvcvideo"
