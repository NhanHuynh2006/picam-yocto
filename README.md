# picam-yocto

Camera phát hiện người thời gian thực trên Raspberry Pi (Zero 2 W hoặc Pi 5) + webcam USB,
chạy trên hệ điều hành Linux tối giản tự build bằng Yocto Project (scarthgap).

Kế hoạch chi tiết từng bước nằm trong **CLAUDE.md** (Claude Code tự đọc file này).

Nhanh:
```bash
./scripts/host_setup.sh                 # 1 lần
./scripts/export_model.sh               # tạo model ONNX
cp kas/wifi.yml.example kas/wifi.yml    # điền WiFi
./scripts/build.sh pi5                  # hoặc: pizero2w
./scripts/flash.sh pi5 /dev/sdX
# cắm thẻ + webcam, mở http://picam.local:8080
```
