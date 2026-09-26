#!/usr/bin/env bash
# Chạy scripts/device/measure.sh trên board qua SSH (không cần copy file sang board)
# và lưu kết quả về laptop: results/<nhãn>.txt
#   ./scripts/measure_remote.sh <nhãn> [số_giây] [user@host]
#   ví dụ: ./scripts/measure_remote.sh yocto-gate-on-empty 60
#          ./scripts/measure_remote.sh rpios-gate-off-person 60 pi@192.168.1.60
set -euo pipefail

LABEL="${1:?Dùng: $0 <nhãn> [số_giây] [user@host]}"
DUR="${2:-60}"
TARGET="${3:-root@picam.local}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/results"

echo ">> Đo '$LABEL' trong ${DUR}s trên $TARGET (đóng trình duyệt đang xem camera)"
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$TARGET" \
    sh -s -- "$LABEL" "$DUR" < "$ROOT/scripts/device/measure.sh" \
    | tee "$ROOT/results/$LABEL.txt"
echo ">> Đã lưu results/$LABEL.txt"
