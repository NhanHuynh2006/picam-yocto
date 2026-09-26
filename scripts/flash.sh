#!/usr/bin/env bash
# Ghi image ra thẻ microSD bằng bmaptool (nhanh, chỉ ghi vùng có dữ liệu).
#   ./scripts/flash.sh pi5 /dev/sdX
# CẨN THẬN: chọn sai thiết bị sẽ xoá ổ cứng. Kiểm tra bằng `lsblk` trước.
set -euo pipefail

BOARD="${1:-}"
DEV="${2:-}"
case "$BOARD" in
    pizero2w) MACHINE=raspberrypi0-2w-64 ;;
    pi5)      MACHINE=raspberrypi5 ;;
    *) echo "Dùng: $0 <pizero2w|pi5> /dev/sdX"; exit 1 ;;
esac
[ -b "$DEV" ] || { echo "LỖI: $DEV không phải block device"; exit 1; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="$ROOT/build/tmp/deploy/images/$MACHINE"
IMG="$DIR/picam-image-$MACHINE.rootfs.wic.bz2"
[ -f "$IMG" ] || IMG="$(ls -t "$DIR"/picam-image-"$MACHINE"*.wic.bz2 2>/dev/null | head -1)"
[ -f "$IMG" ] || { echo "LỖI: chưa có image, chạy ./scripts/build.sh $BOARD"; exit 1; }
BMAP="${IMG%.bz2}"; BMAP="${BMAP%.wic}.wic.bmap"

# Chặn ghi nhầm vào ổ hệ thống
ROOT_DISK="$(lsblk -no PKNAME "$(findmnt -no SOURCE /)" 2>/dev/null || true)"
if [ -n "$ROOT_DISK" ] && [ "/dev/$ROOT_DISK" = "$DEV" ]; then
    echo "LỖI: $DEV là ổ chứa hệ điều hành đang chạy!"; exit 1
fi

lsblk -o NAME,SIZE,MODEL,MOUNTPOINTS "$DEV"
echo
echo "Sẽ XOÁ TOÀN BỘ $DEV và ghi: $(basename "$IMG")"
read -r -p "Gõ 'yes' để tiếp tục: " ok
[ "$ok" = "yes" ] || { echo "Huỷ."; exit 1; }

for p in $(lsblk -ln -o NAME "$DEV" | tail -n +2); do sudo umount "/dev/$p" 2>/dev/null || true; done
sudo bmaptool copy --bmap "$BMAP" "$IMG" "$DEV"
sync
echo ">> Xong. Cắm thẻ vào Pi, cắm webcam USB, cấp nguồn."
