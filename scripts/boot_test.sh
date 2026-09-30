#!/usr/bin/env bash
# Đo thời gian boot -> phát hiện người đầu tiên N lần bằng cách reboot board qua SSH.
# Phải có người đứng sẵn trong khung hình suốt lúc đo. Kết quả: results/<nhãn>.txt
#   ./scripts/boot_test.sh <nhãn> [số_lần] [user@host]
#   ví dụ: ./scripts/boot_test.sh yocto-boot 5
# Reboot mềm thay cho rút điện: uptime vẫn tính từ lúc kernel chạy nên số đo tương đương.
set -uo pipefail
export LC_ALL=C   # awk phải dùng dấu chấm thập phân, không theo locale máy

LABEL="${1:?Dùng: $0 <nhãn> [số_lần] [user@host]}"
RUNS="${2:-5}"
TARGET="${3:-root@picam.local}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/results/$LABEL.txt"
SSH=(ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR
     -o ConnectTimeout=5 "$TARGET")
SUDO=""; [ "${TARGET%%@*}" = root ] || SUDO="sudo"
mkdir -p "$ROOT/results"
: > "$OUT"

for run in $(seq 1 "$RUNS"); do
    echo ">> Lần $run/$RUNS: reboot..."
    "${SSH[@]}" "$SUDO reboot" 2>/dev/null
    sleep 15
    line=""
    for _ in $(seq 1 40); do
        line="$("${SSH[@]}" 'journalctl -u picam-detector -b --no-pager -o cat | grep FIRST_PERSON_DETECTED' 2>/dev/null)"
        [ -n "$line" ] && break
        sleep 3
    done
    {
        echo "run=$run"
        "${SSH[@]}" 'systemd-analyze | head -n 1; journalctl -u picam-detector -b --no-pager -o cat | grep -E "START|CAMERA_OPENED|MODEL_READY|FIRST_PERSON_DETECTED"' 2>/dev/null
        [ -n "$line" ] || echo "KHÔNG có FIRST_PERSON_DETECTED sau 2 phút (có người trong khung hình không?)"
    } | tee -a "$OUT"
done

echo "# --- trung bình ---" | tee -a "$OUT"
grep -oE 'FIRST_PERSON_DETECTED uptime=[0-9.]+' "$OUT" | grep -oE '[0-9.]+$' \
    | awk '{s+=$1; n++} END{if(n) printf "boot_to_first_person_s=%.2f (n=%d)\n", s/n, n}' | tee -a "$OUT"
grep -oE '= [0-9.]+s' "$OUT" | grep -oE '[0-9.]+' \
    | awk '{s+=$1; n++} END{if(n) printf "systemd_startup_s=%.2f (n=%d)\n", s/n, n}' | tee -a "$OUT"
echo ">> Đã lưu results/$LABEL.txt"
