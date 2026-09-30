SUMMARY = "ONNX person-detection model (YOLO26n, Ultralytics) for picam-detector"
# Trọng số YOLO của Ultralytics phát hành theo AGPL-3.0
LICENSE = "AGPL-3.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/AGPL-3.0-only;md5=73f1eb20517c55bf9493b7dd6e480788"

# Kích thước đầu vào do file kas/<board>.yml quyết định; file model phải được
# tạo trước bằng scripts/export_model.sh
PICAM_INPUT_SIZE ?= "320"

SRC_URI = "file://model-${PICAM_INPUT_SIZE}.onnx"
S = "${WORKDIR}"

inherit allarch

do_install() {
    install -d ${D}${datadir}/picam
    install -m 0644 ${WORKDIR}/model-${PICAM_INPUT_SIZE}.onnx ${D}${datadir}/picam/model.onnx
}

FILES:${PN} = "${datadir}/picam"
