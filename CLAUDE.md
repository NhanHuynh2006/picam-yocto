# Đồ án Hệ thống nhúng — Camera phát hiện người trên Linux tự build bằng Yocto

Tài liệu này là **kế hoạch làm việc cho Claude Code**. Làm lần lượt từng giai đoạn, kiểm tra
"Điều kiện hoàn thành" trước khi sang giai đoạn sau. Trả lời người dùng bằng tiếng Việt.

## Đề tài

**Xây dựng hệ điều hành Linux nhúng tối giản bằng Yocto Project cho camera phát hiện người
thời gian thực trên Raspberry Pi.**

Phần cứng: Raspberry Pi (Zero 2 W **hoặc** Pi 5 — chưa chốt) + **webcam USB**. Không có LED,
còi, màn hình hay linh kiện nào khác.

Sản phẩm cần có:
1. Image Yocto (layer riêng `meta-picam`) boot lên là tự chạy ứng dụng phát hiện người.
2. Ứng dụng C++ `picam-detector`: đọc webcam (V4L2) → YOLOv8n (OpenCV DNN) → web xem trực
   tiếp tại `http://<ip>:8080` + số liệu JSON tại `/stats`.
3. Số liệu so sánh image Yocto với Raspberry Pi OS (cùng board, cùng app, cùng model).

## Quy tắc bắt buộc

- Nhánh Yocto cố định là **scarthgap** (5.0 LTS). Không đổi nhánh, không thêm layer ngoài
  poky / meta-oe / meta-raspberrypi nếu người dùng chưa đồng ý.
- Chỉ sửa trong repo này: `kas/`, `meta-picam/`, `scripts/`, `tools/`. **Không sửa** các thư mục
  `poky/`, `meta-openembedded/`, `meta-raspberrypi/` do kas tự clone. Cần chỉnh recipe ngoài
  thì tạo `.bbappend` trong `meta-picam`.
- Đổi board **chỉ** bằng tham số `pizero2w` / `pi5` của script. Không chép cấu hình ra chỗ khác.
- Bước có nhãn **[NGƯỜI DÙNG]** cần phần cứng (thẻ nhớ, cắm board, đứng trước camera): dừng
  lại, nói rõ người dùng phải làm gì, chờ họ báo kết quả rồi mới làm tiếp.
- Không commit `kas/wifi.yml` (mật khẩu WiFi) và file `*.onnx` (đã có trong `.gitignore`).
- Lệnh build Yocto chạy hàng giờ: chạy với thời gian chờ đủ dài hoặc chạy nền và ghi log ra
  file (`./scripts/build.sh pi5 2>&1 | tee build-pi5.log`), không được ngắt giữa chừng.
- Lỗi build: đọc log của task lỗi (`build/tmp/work/.../temp/log.do_<task>`), sửa nguyên nhân
  gốc. Không tắt QA check, không xoá `cache/` khi chưa cần.
- Không thêm tính năng ngoài phạm vi trên (không GPIO, không cloud, không giao diện desktop).

## Cấu trúc repo

```
kas/common.yml            cấu hình chung: layer, systemd, OpenCV tối giản, giới hạn RAM khi build
kas/pizero2w.yml          MACHINE=raspberrypi0-2w-64, camera 320x240, model 256
kas/pi5.yml               MACHINE=raspberrypi5,       camera 640x480, model 320
kas/wifi.yml.example      mẫu cấu hình WiFi -> copy thành kas/wifi.yml
meta-picam/
  conf/layer.conf
  recipes-core/images/picam-image.bb                  image: boot + mạng + SSH + app
  recipes-apps/picam-detector/                        app C++ (CMake) + service systemd + cấu hình
  recipes-apps/picam-model/                           cài model ONNX vào /usr/share/picam/
  recipes-connectivity/network-config/                systemd-networkd + wpa_supplicant
scripts/host_setup.sh     cài Docker + kas trên máy build (1 lần)
scripts/export_model.sh   xuất YOLOv8n -> ONNX (256, 320) và tự kiểm tra bằng OpenCV
scripts/build.sh          build image cho một board
scripts/flash.sh          ghi image ra thẻ nhớ bằng bmaptool
scripts/check_device.sh   từ laptop: kiểm tra hết giai đoạn 4 bằng một lệnh (SSH + /stats + snapshot)
scripts/measure_remote.sh từ laptop: chạy measure.sh trên board qua SSH, lưu results/<nhãn>.txt
scripts/rpios/install_baseline.sh   cài cùng app lên Raspberry Pi OS (baseline + thử khả thi)
scripts/device/measure.sh           đo trên thiết bị (chạy được trên cả 2 hệ điều hành)
tools/collect_stats.py    lấy /stats từ laptop ra CSV
tools/plot_results.py     vẽ biểu đồ từ results/results.csv
```

## Chọn / đổi board

| | Pi Zero 2 W | Pi 5 |
|---|---|---|
| Lệnh build | `./scripts/build.sh pizero2w` | `./scripts/build.sh pi5` |
| MACHINE | `raspberrypi0-2w-64` | `raspberrypi5` |
| Camera | 320x240 | 640x480 |
| Model | `model-256.onnx` | `model-320.onnx` |
| Mạng | chỉ WiFi 2.4 GHz → **bắt buộc** `kas/wifi.yml` | WiFi hoặc cáp LAN |
| Nối webcam | qua đầu chuyển OTG micro-USB → USB-A, cắm cổng **USB** (không phải PWR) | cổng USB bất kỳ |

`cache/downloads` và `cache/sstate` dùng chung giữa hai board, nên đổi board chỉ build lại phần
khác biệt (kernel, firmware), không phải từ đầu.

---

## Giai đoạn 0 — Chuẩn bị máy build

```bash
./scripts/host_setup.sh
git init && git add -A && git commit -m "Khởi tạo đồ án picam"   # kas cần repo git
cp kas/wifi.yml.example kas/wifi.yml                              # rồi hỏi người dùng SSID/mật khẩu để điền
```

Build chạy trong `kas-container` (Docker), nên không phụ thuộc phiên bản Ubuntu của máy host.

**Điều kiện hoàn thành:** `kas-container --version` chạy được không cần sudo; còn ≥ 100 GB trống;
`kas/wifi.yml` đã điền (bắt buộc nếu dùng Pi Zero 2 W).

## Giai đoạn 1 — Xuất model

```bash
./scripts/export_model.sh          # tạo model-256.onnx và model-320.onnx
```

**Điều kiện hoàn thành:** in ra hai dòng `>> OK ... output=(1, 84, N)`.

Model phải là dạng đầu ra YOLOv8 `[1, 4+số_lớp, N]`. OpenCV **4.6 không chạy được** model này;
cần OpenCV ≥ 4.7 (Yocto scarthgap có 4.9, Raspberry Pi OS Trixie có 4.10).

## Giai đoạn 2 — Thử khả thi + baseline trên Raspberry Pi OS  [NGƯỜI DÙNG]

Mục đích kép: (a) biết Pi Zero 2 W có chạy nổi không trước khi tốn giờ build Yocto,
(b) lấy số liệu baseline để so sánh.

1. [NGƯỜI DÙNG] Dùng Raspberry Pi Imager ghi **Raspberry Pi OS Lite 64-bit (bản Trixie)**, trong
   phần tuỳ chỉnh bật SSH và điền WiFi. Không dùng bản Bookworm (OpenCV 4.6 quá cũ).
2. Copy repo sang Pi (không kèm build/cache):
   ```bash
   rsync -av --exclude build --exclude cache --exclude poky --exclude meta-openembedded \
         --exclude meta-raspberrypi --exclude .venv-export ./ <user>@<ip-pi>:~/picam-yocto/
   ```
3. Trên Pi: `cd ~/picam-yocto && sudo ./scripts/rpios/install_baseline.sh pizero2w` (hoặc `pi5`).
4. [NGƯỜI DÙNG] Mở `http://<ip-pi>:8080`, đứng trước camera, xác nhận khung vàng hiện đúng.
5. Đo nhanh khi có người trong khung hình: `sh scripts/device/measure.sh rpios-check 60`.

**Quy tắc chọn board:** giữ Pi Zero 2 W nếu `infer_ms_mean ≤ 500` (≥ 2 lần/giây) và
`mem_available_mb ≥ 60` khi đang chạy; ngược lại chuyển sang Pi 5. Báo số liệu và đề xuất cho
người dùng, **chờ người dùng chốt** rồi mới sang giai đoạn 3.

## Giai đoạn 3 — Build image Yocto

```bash
./scripts/build.sh <board> 2>&1 | tee build-<board>.log
```

Lần đầu mất khoảng 2–4 giờ (OpenCV là phần lâu nhất). Debug nhanh một recipe:
`./scripts/build.sh <board> picam-detector`. Mở shell bitbake để điều tra:
`kas-container shell kas/common.yml:kas/<board>.yml`.

**Điều kiện hoàn thành:** có file `build/tmp/deploy/images/<MACHINE>/picam-image-<MACHINE>.rootfs.wic.bz2`
và `.wic.bmap`. Ghi lại kích thước file `.wic.bz2` và kích thước rootfs
(`du -sh build/tmp/work/*/picam-image/1.0/rootfs`) cho báo cáo.

## Giai đoạn 4 — Ghi thẻ và kiểm thử trên board  [NGƯỜI DÙNG]

1. [NGƯỜI DÙNG] Cắm thẻ nhớ vào laptop, xem tên thiết bị bằng `lsblk`.
2. `./scripts/flash.sh <board> /dev/sdX` (script bắt gõ `yes`, chặn ghi nhầm ổ hệ thống).
3. [NGƯỜI DÙNG] Cắm thẻ + webcam vào Pi, cấp nguồn, đợi ~1 phút.
4. `ssh root@picam.local` (không mật khẩu). Nếu `picam.local` không phân giải được thì lấy IP
   trong trang quản lý router. Kiểm tra lần lượt:
   ```bash
   systemctl status picam-detector
   journalctl -u picam-detector -b --no-pager | head -30
   v4l2-ctl --list-devices          # phải thấy webcam /dev/video0
   ip -4 addr                       # busybox không có `ip -brief`
   ```
5. [NGƯỜI DÙNG] Mở `http://picam.local:8080`, xác nhận phát hiện đúng.

Có thể làm bước 4 bằng một lệnh trên laptop: `./scripts/check_device.sh [root@<ip>]`.

**Điều kiện hoàn thành:** log có đủ `CAMERA_OPENED`, `MODEL_READY`, và `FIRST_PERSON_DETECTED`
khi có người trước camera; trang web hiện hình và khung vàng.

## Giai đoạn 5 — Đo đạc và so sánh

So sánh **4 cấu hình trên cùng một board, cùng webcam, cùng vị trí camera, cùng ánh sáng**:
`rpios-gate-off`, `rpios-gate-on`, `yocto-gate-off`, `yocto-gate-on`.

Bật/tắt cổng chuyển động trên thiết bị:
```bash
sed -i 's/^PICAM_MOTION_GATE=.*/PICAM_MOTION_GATE=0/' /etc/default/picam && systemctl restart picam-detector
```

Mỗi hệ điều hành:
1. **Thời gian boot → phát hiện đầu tiên** [NGƯỜI DÙNG]: đứng sẵn trong khung hình, cấp nguồn,
   giữ nguyên đến khi có người được phát hiện. Lặp **5 lần**, mỗi lần lấy dòng
   `FIRST_PERSON_DETECTED uptime=...` (`journalctl -u picam-detector -b | grep FIRST`) và dòng
   đầu của `systemd-analyze`. Ghi trung bình. (Uptime tính từ lúc kernel chạy; thời gian firmware
   của Pi giống nhau ở hai hệ điều hành nên so sánh vẫn công bằng. Muốn tính cả firmware thì quay
   video từ lúc cắm điện.)
2. **Hiệu năng khi có người** [NGƯỜI DÙNG đứng trước camera]:
   `sh measure.sh <os>-gate-off-person 60`.
3. **Mức tải khi không có ai** (đây là chỗ cổng chuyển động tạo khác biệt): để phòng trống,
   đo `sh measure.sh <os>-gate-off-empty 60` và `sh measure.sh <os>-gate-on-empty 60`.
4. Đóng trình duyệt khi đo (mã hoá JPEG cho trình duyệt cũng tốn CPU).
   Hoặc từ laptop, không cần copy script sang board: `./scripts/measure_remote.sh <nhãn> 60 [user@host]`
   (kết quả lưu thẳng vào `results/<nhãn>.txt`).
5. Lấy file kết quả về laptop (dropbear không có SFTP nên dùng `cat`):
   ```bash
   ssh root@picam.local cat /tmp/picam-measure-<nhãn>.txt > results/<nhãn>.txt
   ```
   Có thể ghi thêm chuỗi thời gian: `python3 tools/collect_stats.py --host picam.local --seconds 60 --out results/<nhãn>.csv`.

Lưu ý: trên image Yocto `/var/log` và `/tmp` nằm trên RAM, reboot là mất — lấy file về trước
khi tắt máy.

## Giai đoạn 6 — Tổng hợp cho báo cáo

1. Copy `results/results_template.csv` thành `results/results.csv`, điền số liệu từ các file đo.
2. `pip install matplotlib && python3 tools/plot_results.py results/results.csv` → ảnh PNG trong `results/`.
3. Viết `results/SUMMARY.md`: bảng số liệu, nhận xét Yocto so với Raspberry Pi OS (boot, RAM,
   dung lượng, tốc độ AI) và tác dụng của cổng chuyển động (CPU%, nhiệt độ khi phòng trống).
   Chỉ nêu kết luận có số liệu chứng minh.

---

## Tham số ứng dụng (`/etc/default/picam`)

| Biến | Mặc định | Ý nghĩa |
|---|---|---|
| `PICAM_CAMERA` | `/dev/video0` | thiết bị camera (đường dẫn hoặc số) |
| `PICAM_CAM_WIDTH/HEIGHT/FPS` | theo board | độ phân giải xin từ webcam (MJPG) |
| `PICAM_MODEL` | `/usr/share/picam/model.onnx` | model ONNX dạng YOLOv8 |
| `PICAM_INPUT_SIZE` | theo board | phải **khớp** kích thước lúc export model |
| `PICAM_CLASS_ID` | `0` | lớp cần phát hiện (COCO: 0 = người) |
| `PICAM_CONF` / `PICAM_NMS` | `0.40` / `0.45` | ngưỡng tin cậy / NMS |
| `PICAM_THREADS` | `0` | số luồng OpenCV (0 = tất cả nhân) |
| `PICAM_MOTION_GATE` | `1` | chỉ chạy AI khi có chuyển động |
| `PICAM_MOTION_THRESHOLD` | `0.005` | tỉ lệ điểm ảnh thay đổi để coi là có chuyển động |
| `PICAM_MOTION_KEEPALIVE_MS` | `2000` | dù không chuyển động vẫn chạy AI tối thiểu mỗi 2 s |
| `PICAM_PERSON_HOLD_MS` | `3000` | sau khi thấy người, chạy AI liên tục thêm 3 s |
| `PICAM_HTTP_PORT` | `8080` | cổng web |
| `PICAM_STREAM_FPS` | theo board | tốc độ khung hình gửi lên trình duyệt |
| `PICAM_CSV_LOG` | `/var/log/picam/infer.csv` | log từng lần suy luận (để đo) |

## Kiến trúc ứng dụng (dùng cho báo cáo)

Bốn luồng: **camera** (luôn giữ khung hình mới nhất, không xếp hàng) → **suy luận** (cổng
chuyển động bằng trừ ảnh 160x120, rồi YOLOv8n qua OpenCV DNN với letterbox + NMS) →
**web** (MJPEG, mã hoá JPEG một lần dùng chung cho mọi người xem) và **main** (tính tốc độ, in
trạng thái mỗi 10 s). Camera được mở song song với lúc nạp model để rút ngắn thời gian từ boot
đến lần phát hiện đầu tiên. Dịch vụ systemd cố ý không chờ mạng vì cùng lý do.

Liên hệ nội dung môn học: Yocto layer/recipe/image (chương 7), biên dịch chéo bằng toolchain
Yocto (chương 4), cấu hình kernel/firmware Raspberry Pi qua BSP layer (chương 5), giao tiếp
thiết bị ngoại vi USB qua V4L2 (chương 6).

## Sự cố thường gặp

| Triệu chứng | Cách xử lý |
|---|---|
| `permission denied ... docker.sock` | chưa đăng nhập lại sau khi thêm vào nhóm docker (`newgrp docker`) |
| Máy build treo/hết RAM khi build OpenCV | trong `kas/common.yml` giảm `BB_NUMBER_THREADS` và `PARALLEL_MAKE` xuống 4 |
| Lỗi tải mã nguồn (fetch) | mạng chập chờn, chạy lại lệnh build (phần đã tải được giữ trong cache) |
| `Unable to get checksum for picam-model SRC_URI entry model-XXX.onnx` | chưa chạy `./scripts/export_model.sh` |
| Pi Zero 2 W không vào WiFi | kiểm tra `kas/wifi.yml`, mạng phải là **2.4 GHz**, build lại `network-config` rồi image |
| Không thấy `/dev/video0` | `dmesg | grep -i uvc`; với Zero kiểm tra đầu OTG cắm đúng cổng USB, không phải cổng PWR |
| App báo `FATAL: cannot load model` | sai đường dẫn model, hoặc `PICAM_INPUT_SIZE` không khớp model |
| Lỗi `shape_utils.hpp ... Assertion failed` | OpenCV 4.6 (Raspberry Pi OS Bookworm) — dùng bản Trixie |
| Pi 5 báo thiếu nguồn / webcam chập chờn | dùng nguồn 5V 5A chính hãng; nguồn 3A giới hạn dòng cổng USB |
| Web không mở | `ip -4 addr` trên Pi, dùng IP thay cho `picam.local` (Windows hay không phân giải được .local) |
