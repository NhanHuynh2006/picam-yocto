#!/usr/bin/env bash
# Chỉ cần khi Raspberry Pi OS là bản Bookworm (OpenCV 4.6 không chạy được model YOLOv8).
# Biên dịch OpenCV 4.9.0 — CÙNG phiên bản với image Yocto scarthgap — vào /opt/opencv-4.9.0,
# không đụng tới gói OpenCV của hệ thống. Cấu hình module giống kas/common.yml:
# core/imgproc/imgcodecs/videoio(V4L2)/dnn, không TBB, không GStreamer.
#
# Chạy TRÊN Raspberry Pi:  sudo ./scripts/rpios/build_opencv.sh
# Sau đó chạy install_baseline.sh như thường (tự nhận /opt/opencv-4.9.0).
set -euo pipefail

VER=4.9.0
PREFIX=/opt/opencv-$VER
[ "$(id -u)" = 0 ] || { echo "Cần chạy bằng sudo"; exit 1; }
if [ -f "$PREFIX/lib/cmake/opencv4/OpenCVConfig.cmake" ]; then
    echo ">> Đã có $PREFIX, bỏ qua."; exit 0
fi

apt-get update
apt-get install -y --no-install-recommends cmake g++ make pkg-config curl ca-certificates \
    libjpeg-dev libpng-dev zlib1g-dev libv4l-dev

WORK=/tmp/opencv-build
mkdir -p "$WORK" && cd "$WORK"
[ -d "opencv-$VER" ] || curl -fsSL "https://github.com/opencv/opencv/archive/refs/tags/$VER.tar.gz" | tar xz

cmake -S "opencv-$VER" -B build \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn \
    -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF \
    -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF \
    -DWITH_V4L=ON -DWITH_LIBV4L=ON -DWITH_JPEG=ON -DWITH_PNG=ON \
    -DWITH_TBB=OFF -DWITH_OPENMP=OFF -DWITH_GSTREAMER=OFF -DWITH_FFMPEG=OFF -DWITH_GTK=OFF \
    -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENEXR=OFF -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF \
    -DWITH_OPENCL=OFF -DWITH_ITT=OFF -DWITH_PROTOBUF=ON -DBUILD_PROTOBUF=ON
cmake --build build -j"$(nproc)"
cmake --install build
rm -rf "$WORK"
echo ">> OK: OpenCV $VER tại $PREFIX"
