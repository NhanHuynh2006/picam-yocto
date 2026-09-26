#include "state.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "util.hpp"

namespace {
// Same yellow as the web page accent (#F5C542), in BGR.
const cv::Scalar kBoxColor(66, 197, 245);

std::string numOrNull(double v, const char* fmt) {
    if (v < 0) return "null";
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt, v);
    return buf;
}
}  // namespace

void SharedState::publishFrame(cv::Mat&& frame) {
    {
        std::lock_guard<std::mutex> lk(frame_mtx_);
        frame_ = std::move(frame);
        ++frame_seq_;
    }
    frame_cv_.notify_all();
}

bool SharedState::waitForNewFrame(std::uint64_t last_seq, cv::Mat& out, std::uint64_t& seq,
                                  int timeout_ms) {
    std::unique_lock<std::mutex> lk(frame_mtx_);
    frame_cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                       [&] { return frame_seq_ > last_seq || !running.load(); });
    if (frame_seq_ <= last_seq || frame_.empty()) return false;
    out = frame_;  // shallow copy (refcounted), buffer is never modified afterwards
    seq = frame_seq_;
    return true;
}

std::uint64_t SharedState::frameCount() {
    std::lock_guard<std::mutex> lk(frame_mtx_);
    return frame_seq_;
}

void SharedState::publishDetections(std::vector<Detection> dets) {
    std::lock_guard<std::mutex> lk(det_mtx_);
    dets_ = std::move(dets);
}

bool SharedState::annotatedJpeg(std::vector<uchar>& out, int quality) {
    std::lock_guard<std::mutex> jlk(jpeg_mtx_);

    cv::Mat frame;
    std::uint64_t seq = 0;
    {
        std::lock_guard<std::mutex> lk(frame_mtx_);
        frame = frame_;
        seq = frame_seq_;
    }
    if (frame.empty()) return false;
    if (seq == jpeg_seq_ && !jpeg_cache_.empty()) {
        out = jpeg_cache_;
        return true;
    }

    std::vector<Detection> dets;
    {
        std::lock_guard<std::mutex> lk(det_mtx_);
        dets = dets_;
    }
    double infer_ms = 0.0, infer_fps = 0.0;
    {
        std::lock_guard<std::mutex> lk(stats_mtx);
        infer_ms = stats.infer_ms_avg;
        infer_fps = stats.infer_fps;
    }

    cv::Mat canvas = frame.clone();
    const int thickness = canvas.cols >= 480 ? 2 : 1;
    const double font = canvas.cols >= 480 ? 0.5 : 0.35;
    for (const auto& d : dets) {
        cv::rectangle(canvas, d.box, kBoxColor, thickness);
        char label[32];
        std::snprintf(label, sizeof(label), "person %.2f", d.score);
        int base = 0;
        cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font, 1, &base);
        cv::Point org(d.box.x, std::max(ts.height + 2, d.box.y - 3));
        cv::rectangle(canvas, cv::Rect(org.x, org.y - ts.height - 2, ts.width + 4, ts.height + 4),
                      kBoxColor, cv::FILLED);
        cv::putText(canvas, label, cv::Point(org.x + 2, org.y), cv::FONT_HERSHEY_SIMPLEX, font,
                    cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
    }

    char hud[96];
    std::snprintf(hud, sizeof(hud), "AI %.0f ms | %.1f fps | %zu person", infer_ms, infer_fps,
                  dets.size());
    int base = 0;
    cv::Size ts = cv::getTextSize(hud, cv::FONT_HERSHEY_SIMPLEX, font, 1, &base);
    cv::rectangle(canvas, cv::Rect(0, 0, ts.width + 10, ts.height + 10), cv::Scalar(0, 0, 0),
                  cv::FILLED);
    cv::putText(canvas, hud, cv::Point(5, ts.height + 4), cv::FONT_HERSHEY_SIMPLEX, font,
                cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
    if (!cv::imencode(".jpg", canvas, jpeg_cache_, params)) return false;
    jpeg_seq_ = seq;
    out = jpeg_cache_;
    return true;
}

std::string SharedState::statsJson() {
    Stats s;
    {
        std::lock_guard<std::mutex> lk(stats_mtx);
        s = stats;
    }
    char buf[1024];
    std::snprintf(
        buf, sizeof(buf),
        "{\"uptime_s\":%.2f,\"app_start_uptime_s\":%s,\"camera_open_uptime_s\":%s,"
        "\"ready_uptime_s\":%s,\"first_person_uptime_s\":%s,\"model_load_ms\":%.1f,"
        "\"camera_ok\":%s,\"capture_fps\":%.2f,\"infer_fps\":%.2f,\"infer_ms_last\":%.1f,"
        "\"infer_ms_avg\":%.1f,\"persons\":%d,\"inferences\":%llu,\"skipped_motion\":%llu,"
        "\"motion_frac\":%.4f,\"stream_clients\":%d,\"cpu_temp_c\":%s,\"mem_available_mb\":%ld}",
        util::uptimeSeconds(), numOrNull(s.app_start_uptime, "%.2f").c_str(),
        numOrNull(s.camera_open_uptime, "%.2f").c_str(), numOrNull(s.ready_uptime, "%.2f").c_str(),
        numOrNull(s.first_person_uptime, "%.2f").c_str(), s.model_load_ms,
        s.camera_ok ? "true" : "false", s.capture_fps, s.infer_fps, s.infer_ms_last,
        s.infer_ms_avg, s.persons, static_cast<unsigned long long>(s.inferences),
        static_cast<unsigned long long>(s.skipped_motion), s.motion_frac, stream_clients.load(),
        numOrNull(util::cpuTempC(), "%.1f").c_str(), util::memAvailableMb());
    return buf;
}

void SharedState::wakeAll() { frame_cv_.notify_all(); }
