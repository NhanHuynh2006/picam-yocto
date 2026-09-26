#!/usr/bin/env python3
"""Vẽ biểu đồ so sánh từ results/results.csv (mỗi dòng = một cấu hình đã đo).

    pip install matplotlib
    python3 tools/plot_results.py results/results.csv
Ảnh PNG được lưu vào results/.
"""
import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

METRICS = {
    "boot_to_first_person_s": "Từ lúc kernel khởi động đến lần phát hiện người đầu tiên (s)",
    "userspace_ready_s": "Thời gian khởi động theo systemd-analyze (s)",
    "mem_available_mb": "RAM còn trống khi đang chạy (MB)",
    "rootfs_used_mb": "Dung lượng hệ điều hành trên thẻ nhớ (MB)",
    "infer_ms_mean": "Thời gian một lần suy luận (ms)",
    "infer_per_s": "Số lần suy luận mỗi giây",
    "cpu_util_pct": "Mức dùng CPU (%)",
    "cpu_temp_c": "Nhiệt độ SoC (°C)",
}


def main() -> None:
    src = Path(sys.argv[1] if len(sys.argv) > 1 else "results/results.csv")
    rows = list(csv.DictReader(src.open()))
    if not rows:
        raise SystemExit(f"{src} không có dữ liệu")
    labels = [r["config"] for r in rows]

    for key, title in METRICS.items():
        values = []
        for r in rows:
            try:
                values.append(float(r.get(key, "")))
            except ValueError:
                values.append(None)
        if all(v is None for v in values):
            continue
        fig, ax = plt.subplots(figsize=(7, 3.8))
        xs = [l for l, v in zip(labels, values) if v is not None]
        ys = [v for v in values if v is not None]
        bars = ax.bar(xs, ys, color="#3b6e8f")
        ax.bar_label(bars, fmt="%.1f")
        ax.set_title(title, fontsize=11)
        ax.spines[["top", "right"]].set_visible(False)
        plt.xticks(rotation=15, ha="right")
        fig.tight_layout()
        out = src.parent / f"{key}.png"
        fig.savefig(out, dpi=150)
        plt.close(fig)
        print(f"-> {out}")


if __name__ == "__main__":
    main()
