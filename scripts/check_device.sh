#!/usr/bin/env bash
# Kiểm tra toàn bộ giai đoạn 4 từ LAPTOP bằng một lệnh (chạy được cho cả Yocto và Raspberry Pi OS).
#   ./scripts/check_device.sh                    # mặc định root@picam.local (image Yocto)
#   ./scripts/check_device.sh root@192.168.1.50
#   ./scripts/check_device.sh pi@192.168.1.60    # Raspberry Pi OS
# Muốn kiểm tra FIRST_PERSON_DETECTED: đứng trước camera trước khi chạy.
set -uo pipefail

TARGET="${1:-root@picam.local}"
HOST="${TARGET#*@}"
PORT="${PICAM_HTTP_PORT:-8080}"
# Mỗi lần ghi lại thẻ, board sinh host key mới -> không lưu/kiểm tra host key
SSH=(ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR
     -o ConnectTimeout=10 "$TARGET")

PASS=0; FAIL=0
ok()   { echo "  [OK]   $*"; PASS=$((PASS + 1)); }
bad()  { echo "  [LỖI]  $*"; FAIL=$((FAIL + 1)); }

echo ">> Kết nối $TARGET"
if ! "${SSH[@]}" true; then
    echo "Không SSH được. Thử: ping $HOST; hoặc lấy IP trong trang quản lý router rồi chạy lại với root@<ip>."
    exit 1
fi

echo ">> Thông tin hệ thống"
"${SSH[@]}" 'tr -d "\0" < /proc/device-tree/model; echo; uname -r; systemd-analyze | head -n 1; ip -4 -o addr | grep -v " lo "'

echo ">> Dịch vụ picam-detector"
state="$("${SSH[@]}" systemctl is-active picam-detector)"
[ "$state" = active ] && ok "systemctl: active" || bad "systemctl: $state"

echo ">> Webcam"
v4l="$("${SSH[@]}" 'v4l2-ctl --list-devices 2>&1')"
echo "$v4l" | sed 's/^/     /'
echo "$v4l" | grep -q /dev/video0 && ok "/dev/video0 có mặt" \
    || bad "không thấy /dev/video0 (dmesg | grep -i uvc; với Zero kiểm tra đầu OTG cắm cổng USB)"

echo ">> Log ứng dụng (lần boot hiện tại)"
log="$("${SSH[@]}" 'journalctl -u picam-detector -b --no-pager -o cat 2>/dev/null')"
echo "$log" | grep -E 'START|OpenCV|CAMERA_OPENED|MODEL_READY|FIRST_PERSON|FATAL|ERROR' | head -n 12 | sed 's/^/     /'
for key in CAMERA_OPENED MODEL_READY FIRST_PERSON_DETECTED; do
    echo "$log" | grep -q "$key" && ok "$key" || bad "chưa có $key"
done
echo "$log" | grep -q FATAL && bad "có dòng FATAL trong log"

echo ">> Web http://$HOST:$PORT"
stats="$(curl -fsS --max-time 5 "http://$HOST:$PORT/stats" 2>&1)"
if [ $? -eq 0 ]; then
    ok "/stats trả về JSON"
    echo "$stats" | python3 -m json.tool 2>/dev/null | sed 's/^/     /'
    curl -fsS --max-time 5 -o /tmp/picam-snapshot.jpg "http://$HOST:$PORT/snapshot.jpg" \
        && ok "/snapshot.jpg -> /tmp/picam-snapshot.jpg (mở ảnh xem khung vàng)" \
        || bad "/snapshot.jpg chưa có ảnh"
else
    bad "không mở được /stats: $stats"
fi

echo
echo ">> Kết quả: $PASS OK, $FAIL LỖI"
echo "   Mở trình duyệt: http://$HOST:$PORT"
[ "$FAIL" -eq 0 ]
