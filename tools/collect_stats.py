#!/usr/bin/env python3
"""Ghi số liệu /stats của picam trong N giây ra CSV (chạy trên laptop, chỉ cần Python chuẩn).

    python3 tools/collect_stats.py --host picam.local --seconds 60 --out results/yocto-pi5.csv
"""
import argparse
import csv
import json
import statistics
import time
import urllib.request

FIELDS = ["uptime_s", "capture_fps", "infer_fps", "infer_ms_last", "infer_ms_avg", "persons",
          "inferences", "skipped_motion", "motion_frac", "cpu_temp_c", "mem_available_mb"]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True, help="IP hoặc picam.local")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    url = f"http://{args.host}:{args.port}/stats"
    rows = []
    t_end = time.time() + args.seconds
    while time.time() < t_end:
        t0 = time.time()
        try:
            with urllib.request.urlopen(url, timeout=3) as r:
                s = json.load(r)
            rows.append({k: s.get(k) for k in FIELDS})
        except OSError as e:
            print(f"không đọc được {url}: {e}")
        time.sleep(max(0.0, 1.0 - (time.time() - t0)))

    if not rows:
        raise SystemExit("Không thu được mẫu nào — kiểm tra IP và dịch vụ picam-detector.")

    with open(args.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(rows)

    def col(k):
        return [r[k] for r in rows if isinstance(r[k], (int, float))]

    print(f"{len(rows)} mẫu -> {args.out}")
    print(f"  camera fps       tb {statistics.mean(col('capture_fps')):.1f}")
    print(f"  AI lần/giây      tb {statistics.mean(col('infer_fps')):.2f}")
    ms = col("infer_ms_last")
    if ms:
        print(f"  thời gian AI ms  tb {statistics.mean(ms):.0f}, max {max(ms):.0f}")
    if col("cpu_temp_c"):
        print(f"  nhiệt độ C       max {max(col('cpu_temp_c')):.1f}")
    print(f"  RAM trống MB     min {min(col('mem_available_mb'))}")


if __name__ == "__main__":
    main()
