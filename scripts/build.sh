#!/usr/bin/env bash
# Build image Yocto cho một board.
#   ./scripts/build.sh pizero2w            # image đầy đủ
#   ./scripts/build.sh pi5                 # image đầy đủ
#   ./scripts/build.sh pi5 picam-detector  # chỉ build một recipe (debug nhanh)
set -euo pipefail

BOARD="${1:-}"
TARGET="${2:-}"
case "$BOARD" in
    pizero2w) MACHINE=raspberrypi0-2w-64 ;;
    pi5)      MACHINE=raspberrypi5 ;;
    *) echo "Dùng: $0 <pizero2w|pi5> [bitbake-target]"; exit 1 ;;
esac

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SIZE="$(grep -E '^\s*PICAM_INPUT_SIZE' "kas/$BOARD.yml" | grep -oE '[0-9]+' | head -1)"
MODEL="meta-picam/recipes-apps/picam-model/files/model-$SIZE.onnx"
if [ ! -f "$MODEL" ]; then
    echo "LỖI: thiếu $MODEL — chạy trước: ./scripts/export_model.sh"
    exit 1
fi

CONFIG="kas/common.yml:kas/$BOARD.yml"
if [ -f kas/wifi.yml ]; then
    CONFIG="$CONFIG:kas/wifi.yml"
else
    echo "CẢNH BÁO: không có kas/wifi.yml — image sẽ không tự vào WiFi (Pi Zero 2 W không có cổng LAN)."
fi

# Cache dùng chung cho mọi board: đổi board không phải tải/biên dịch lại từ đầu
export DL_DIR="$ROOT/cache/downloads"
export SSTATE_DIR="$ROOT/cache/sstate"
mkdir -p "$DL_DIR" "$SSTATE_DIR"

if [ -n "$TARGET" ]; then
    kas-container build --target "$TARGET" "$CONFIG"
else
    kas-container build "$CONFIG"
    echo
    echo ">> Image tại: build/tmp/deploy/images/$MACHINE/"
    ls -lh "build/tmp/deploy/images/$MACHINE/"picam-image-"$MACHINE"*.wic.* 2>/dev/null || true
fi
