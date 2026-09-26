#!/bin/sh
# Đo một cấu hình trên THIẾT BỊ (chạy được trên cả image Yocto lẫn Raspberry Pi OS).
#   sh measure.sh <nhãn> [số_giây]
#   ví dụ: sh measure.sh yocto-pi5-gate-on 60
# In kết quả ra màn hình và lưu /tmp/picam-measure-<nhãn>.txt
set -eu

LABEL="${1:?Dùng: sh measure.sh <nhãn> [số_giây]}"
DUR="${2:-60}"
OUT="/tmp/picam-measure-$LABEL.txt"
CSV="/var/log/picam/infer.csv"

cpu_snapshot() { awk '/^cpu /{idle=$5+$6; t=0; for(i=2;i<=NF;i++) t+=$i; print t, idle}' /proc/stat; }
uptime_s() { cut -d' ' -f1 /proc/uptime; }

{
echo "label=$LABEL"
echo "date=$(date '+%Y-%m-%d %H:%M:%S')"
echo "board=$(tr -d '\0' < /proc/device-tree/model 2>/dev/null || echo unknown)"
echo "kernel=$(uname -r)"
. /etc/os-release 2>/dev/null && echo "os=$PRETTY_NAME"

echo "# --- khởi động (lần boot hiện tại) ---"
systemd-analyze 2>/dev/null | head -n 1 || echo "systemd-analyze: không có"
journalctl -b -u picam-detector --no-pager -o cat 2>/dev/null \
    | grep -E 'START|CAMERA_OPENED|MODEL_READY|FIRST_PERSON_DETECTED' | head -n 4 || true

echo "# --- bộ nhớ, lưu trữ ---"
awk '/^MemTotal/{print "mem_total_mb=" int($2/1024)} /^MemAvailable/{print "mem_available_mb=" int($2/1024)}' /proc/meminfo
PID="$(pidof picam-detector || true)"
[ -n "$PID" ] && awk '/^VmRSS/{print "app_rss_mb=" int($2/1024)}' "/proc/$PID/status"
df -k / | awk 'NR==2{print "rootfs_used_mb=" int($3/1024)}'
echo "processes=$(ls -d /proc/[0-9]* | wc -l)"

echo "# --- đo trong ${DUR}s (giữ nguyên cảnh trước camera, đóng trình duyệt) ---"
set -- $(cpu_snapshot); T1=$1; I1=$2
U1="$(uptime_s)"
sleep "$DUR"
set -- $(cpu_snapshot); T2=$1; I2=$2
U2="$(uptime_s)"
awk -v t=$((T2 - T1)) -v i=$((I2 - I1)) 'BEGIN{ if (t>0) printf "cpu_util_pct=%.1f\n", 100*(t-i)/t }'
[ -r /sys/class/thermal/thermal_zone0/temp ] && awk '{printf "cpu_temp_c=%.1f\n", $1/1000}' /sys/class/thermal/thermal_zone0/temp || echo "cpu_temp_c=unknown"

if [ -f "$CSV" ]; then
    awk -F, -v s="$U1" -v e="$U2" 'NR>1 && $1>=s && $1<=e {print $2}' "$CSV" | sort -n | awk -v d="$DUR" '
        { a[NR]=$1; sum+=$1 }
        END {
            if (NR==0) { print "inferences=0"; exit }
            p95 = a[int(NR*0.95) > 0 ? int(NR*0.95) : 1]
            printf "inferences=%d\ninfer_per_s=%.2f\ninfer_ms_mean=%.1f\ninfer_ms_p95=%.1f\ninfer_ms_max=%.1f\n", NR, NR/d, sum/NR, p95, a[NR]
        }'
else
    echo "Không thấy $CSV (PICAM_CSV_LOG đang tắt?)"
fi
} | tee "$OUT"

echo ">> Đã lưu $OUT"
