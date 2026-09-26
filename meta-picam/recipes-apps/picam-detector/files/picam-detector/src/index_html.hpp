#pragma once

// Status page served at "/". Self-contained: works with no internet access.
static const char kIndexHtml[] = R"HTML(<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>picam</title>
<style>
  :root {
    --bg: #17212b; --panel: #22303c; --line: #33444f;
    --text: #e8eef2; --muted: #93a4b1; --signal: #f5c542; --fault: #ff7a66;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; background: var(--bg); color: var(--text);
    font: 15px/1.5 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  }
  main {
    max-width: 1100px; margin: 0 auto; padding: 20px;
    display: grid; gap: 20px; grid-template-columns: 1fr;
  }
  @media (min-width: 900px) { main { grid-template-columns: 1fr 260px; } }
  header { grid-column: 1 / -1; display: flex; align-items: baseline; gap: 12px; }
  h1 { margin: 0; font-size: 22px; font-weight: 650; letter-spacing: -0.01em; }
  #link { color: var(--muted); font-size: 14px; }
  #link.ok::before { content: ""; display: inline-block; width: 8px; height: 8px;
    border-radius: 50%; background: #5fd08a; margin-right: 6px; }
  #link.down { color: var(--fault); }
  .video { background: #000; border-radius: 6px; overflow: hidden; border: 1px solid var(--line); }
  .video img { display: block; width: 100%; height: auto; }
  aside { background: var(--panel); border-radius: 6px; padding: 18px; align-self: start; }
  .count { font-size: 64px; font-weight: 700; line-height: 1; font-variant-numeric: tabular-nums; }
  .count.seen { color: var(--signal); }
  .count-label { color: var(--muted); margin: 6px 0 18px; }
  dl { margin: 0; display: grid; grid-template-columns: 1fr auto; row-gap: 8px; }
  dt { color: var(--muted); }
  dd { margin: 0; text-align: right; font-variant-numeric: tabular-nums; }
  footer { grid-column: 1 / -1; color: var(--muted); font-size: 13px; }
  a { color: var(--text); }
</style>
</head>
<body>
<main>
  <header><h1>picam</h1><span id="link">đang kết nối…</span></header>
  <div class="video"><img src="/stream" alt="Hình từ camera, khung vàng là người được phát hiện"></div>
  <aside>
    <div id="persons" class="count">0</div>
    <div class="count-label">người trong khung hình</div>
    <dl>
      <dt>Thời gian AI</dt><dd id="ms">–</dd>
      <dt>Tốc độ AI</dt><dd id="fps">–</dd>
      <dt>Camera</dt><dd id="cam">–</dd>
      <dt>Bỏ qua (không chuyển động)</dt><dd id="skip">–</dd>
      <dt>Phát hiện đầu tiên</dt><dd id="first">–</dd>
      <dt>Nhiệt độ</dt><dd id="temp">–</dd>
      <dt>RAM trống</dt><dd id="mem">–</dd>
    </dl>
  </aside>
  <footer>Số liệu thô: <a href="/stats">/stats</a> · Ảnh tĩnh: <a href="/snapshot.jpg">/snapshot.jpg</a></footer>
</main>
<script>
const $ = (id) => document.getElementById(id);
async function tick() {
  try {
    const r = await fetch('/stats', { cache: 'no-store' });
    const s = await r.json();
    $('link').textContent = s.camera_ok ? 'đang chạy' : 'không thấy camera — kiểm tra cáp USB';
    $('link').className = s.camera_ok ? 'ok' : 'down';
    $('persons').textContent = s.persons;
    $('persons').className = s.persons > 0 ? 'count seen' : 'count';
    $('ms').textContent = s.infer_ms_avg.toFixed(0) + ' ms';
    $('fps').textContent = s.infer_fps.toFixed(1) + ' lần/s';
    $('cam').textContent = s.capture_fps.toFixed(1) + ' fps';
    $('skip').textContent = s.skipped_motion;
    $('first').textContent = s.first_person_uptime_s === null ? 'chưa có'
      : s.first_person_uptime_s.toFixed(1) + ' s sau khi boot';
    $('temp').textContent = s.cpu_temp_c === null ? '–' : s.cpu_temp_c.toFixed(1) + ' °C';
    $('mem').textContent = s.mem_available_mb + ' MB';
  } catch (e) {
    $('link').textContent = 'mất kết nối với thiết bị';
    $('link').className = 'down';
  }
}
tick();
setInterval(tick, 1000);
</script>
</body>
</html>
)HTML";
