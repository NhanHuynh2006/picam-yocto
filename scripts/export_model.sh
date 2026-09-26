#!/usr/bin/env bash
# Xuất YOLOv8n sang ONNX cho OpenCV DNN, ở các kích thước đầu vào mà kas/*.yml dùng.
# Chạy trên máy build (laptop). Kết quả: meta-picam/recipes-apps/picam-model/files/model-<size>.onnx
#
#   ./scripts/export_model.sh              # mặc định 256 và 320
#   ./scripts/export_model.sh 224 256 320  # tự chọn kích thước
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/meta-picam/recipes-apps/picam-model/files"
VENV="${VENV:-$ROOT/.venv-export}"
SIZES=("$@")
[ ${#SIZES[@]} -eq 0 ] && SIZES=(256 320)

if [ ! -x "$VENV/bin/python" ]; then
    echo ">> Tạo môi trường Python riêng tại $VENV (bản PyTorch CPU, không cần GPU)"
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install --upgrade pip
    "$VENV/bin/pip" install --index-url https://download.pytorch.org/whl/cpu torch torchvision
    "$VENV/bin/pip" install ultralytics onnx onnxslim
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

"$VENV/bin/python" - "$OUT" "${SIZES[@]}" <<'PY'
import shutil, sys
import cv2
import numpy as np
from ultralytics import YOLO

out_dir, sizes = sys.argv[1], [int(s) for s in sys.argv[2:]]
for size in sizes:
    model = YOLO("yolov8n.pt")  # tự tải trọng số COCO lần đầu
    path = model.export(format="onnx", imgsz=size, opset=12, simplify=True, dynamic=False)
    dst = f"{out_dir}/model-{size}.onnx"
    shutil.copy(path, dst)

    # Kiểm tra: OpenCV DNN đọc được và đầu ra đúng dạng [1, 84, N]
    net = cv2.dnn.readNetFromONNX(dst)
    blob = cv2.dnn.blobFromImage(np.zeros((size, size, 3), np.uint8), 1 / 255.0, (size, size), swapRB=True)
    net.setInput(blob)
    shape = net.forward().shape
    assert len(shape) == 3 and shape[0] == 1 and shape[1] == 84, f"unexpected output {shape}"
    print(f">> OK {dst}  output={shape}  (OpenCV {cv2.__version__})")
PY

ls -lh "$OUT"/model-*.onnx
