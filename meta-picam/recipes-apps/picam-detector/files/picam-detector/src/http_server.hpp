#pragma once

#include "config.hpp"
#include "state.hpp"

// Minimal HTTP/1.0 server (POSIX sockets, no dependencies):
//   /              status page (live video + numbers)
//   /stream        MJPEG stream (multipart/x-mixed-replace)
//   /snapshot.jpg  one annotated frame
//   /stats         JSON metrics (used by tools/collect_stats.py)
void httpServerLoop(const Config& cfg, SharedState& st);
