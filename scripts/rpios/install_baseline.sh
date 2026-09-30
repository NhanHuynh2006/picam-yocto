#!/usr/bin/env bash
# Cài CÙNG ứng dụng lên Raspberry Pi OS Lite 64-bit (bản Trixie) để làm baseline so sánh
# với image Yocto, đồng thời để thử xem board có chạy nổi trước khi build Yocto.
#
# Chạy TRÊN Raspberry Pi, trong bản copy của repo (model đã export sẵn):
#   sudo ./scripts/rpios/install_baseline.sh pizero2w   # hoặc pi5
set -euo pipefail

BOARD="${1:-}"
case "$BOARD" in
    pizero2w) W=320; H=240; SIZE=256; STREAM=5; JOBS=1 ;;   # 512MB RAM: biên dịch 1 luồng
    pi5)      W=640; H=480; SIZE=320; STREAM=10; JOBS=4 ;;
    *) echo "Dùng: sudo $0 <pizero2w|pi5>"; exit 1 ;;
esac
[ "$(id -u)" = 0 ] || { echo "Cần chạy bằng sudo"; exit 1; }

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FILES="$ROOT/meta-picam/recipes-apps/picam-detector/files"
MODEL="$ROOT/meta-picam/recipes-apps/picam-model/files/model-$SIZE.onnx"
[ -f "$MODEL" ] || { echo "LỖI: thiếu $MODEL (export trên laptop rồi copy repo sang)"; exit 1; }

apt-get update
apt-get install -y --no-install-recommends cmake g++ make pkg-config v4l-utils

# Bản Bookworm chỉ có OpenCV 4.6: chạy trước scripts/rpios/build_opencv.sh để có
# OpenCV 4.9.0 (cùng phiên bản với Yocto) tại /opt/opencv-4.9.0.
OPENCV_PREFIX="${OPENCV_PREFIX:-/opt/opencv-4.9.0}"
CMAKE_EXTRA=""
if [ -f "$OPENCV_PREFIX/lib/cmake/opencv4/OpenCVConfig.cmake" ]; then
    echo ">> Dùng OpenCV tại $OPENCV_PREFIX"
    CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=$OPENCV_PREFIX -DCMAKE_INSTALL_RPATH=$OPENCV_PREFIX/lib"
else
    apt-get install -y --no-install-recommends libopencv-dev
    CV="$(pkg-config --modversion opencv4)"
    if [ "$(printf '%s\n4.7.0\n' "$CV" | sort -V | head -1)" != "4.7.0" ]; then
        echo "LỖI: OpenCV $CV quá cũ, không chạy được model YOLOv8 (cần >= 4.7)."
        echo "     Dùng Raspberry Pi OS bản Trixie, hoặc chạy: sudo ./scripts/rpios/build_opencv.sh"
        exit 1
    fi
fi

rm -rf /tmp/picam-build
# shellcheck disable=SC2086
cmake -S "$FILES/picam-detector" -B /tmp/picam-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr $CMAKE_EXTRA
cmake --build /tmp/picam-build -j"$JOBS"
cmake --install /tmp/picam-build

install -d /usr/share/picam
install -m 0644 "$MODEL" /usr/share/picam/model.onnx
sed -e "s|@CAM_WIDTH@|$W|" -e "s|@CAM_HEIGHT@|$H|" -e "s|@INPUT_SIZE@|$SIZE|" -e "s|@STREAM_FPS@|$STREAM|" \
    "$FILES/picam.conf.in" > /etc/default/picam
install -m 0644 "$FILES/picam-detector.service" /etc/systemd/system/picam-detector.service

systemctl daemon-reload
systemctl enable --now picam-detector
sleep 3
journalctl -u picam-detector -b --no-pager | tail -n 15
echo
echo ">> Mở trình duyệt: http://$(hostname -I | awk '{print $1}'):8080"
