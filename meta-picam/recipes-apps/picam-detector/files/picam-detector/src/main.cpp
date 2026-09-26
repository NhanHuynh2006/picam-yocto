// picam-detector: realtime person detection on a Raspberry Pi with a USB camera.
//
// Threads:
//   camera    — V4L2 capture, always keeps only the newest frame
//   inference — motion gate + YOLOv8 (OpenCV DNN) on the newest frame
//   http      — status page, MJPEG stream, JSON stats
//   main      — computes rates, prints a status line every 10 s
//
// Key log lines (grep them from `journalctl -u picam-detector -b`):
//   START, CAMERA_OPENED, MODEL_READY, FIRST_PERSON_DETECTED — all carry
//   "uptime=" = seconds since the kernel started, used for boot-time measurements.

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

#include "camera.hpp"
#include "config.hpp"
#include "detector.hpp"
#include "http_server.hpp"
#include "motion.hpp"
#include "state.hpp"
#include "util.hpp"

namespace {

SharedState* g_state = nullptr;

void onSignal(int) {
    if (g_state) g_state->running.store(false);
}

void inferenceLoop(const Config& cfg, SharedState& st, PersonDetector& det) {
    MotionGate gate;

    FILE* csv = nullptr;
    if (!cfg.csv_log.empty()) {
        csv = std::fopen(cfg.csv_log.c_str(), "a");
        if (!csv) {
            PLOG("WARN: cannot open CSV log %s: %s", cfg.csv_log.c_str(), std::strerror(errno));
        } else if (std::ftell(csv) == 0) {
            std::fprintf(csv, "uptime_s,infer_ms,persons,motion_frac\n");
        }
    }

    std::uint64_t last_seq = 0;
    double last_infer_at = -1e18;
    double last_person_at = -1e18;
    int csv_lines = 0;

    while (st.running.load()) {
        cv::Mat frame;
        std::uint64_t seq = 0;
        if (!st.waitForNewFrame(last_seq, frame, seq, 500)) continue;
        last_seq = seq;

        const double now = util::nowMs();
        const double motion = cfg.motion_gate ? gate.update(frame) : 1.0;
        const bool run = !cfg.motion_gate || motion >= cfg.motion_threshold ||
                         (now - last_person_at) < cfg.person_hold_ms ||
                         (now - last_infer_at) >= cfg.motion_keepalive_ms;
        if (!run) {
            std::lock_guard<std::mutex> lk(st.stats_mtx);
            st.stats.skipped_motion++;
            st.stats.motion_frac = motion;
            continue;
        }

        std::vector<Detection> dets;
        const double t0 = util::nowMs();
        try {
            dets = det.detect(frame);
        } catch (const std::exception& e) {
            PLOG("ERROR: inference failed: %s", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        const double t1 = util::nowMs();
        const double dt = t1 - t0;
        last_infer_at = t1;
        if (!dets.empty()) last_person_at = t1;

        const size_t n = dets.size();
        st.publishDetections(std::move(dets));

        const double up = util::uptimeSeconds();
        bool first = false;
        double app_start = 0.0;
        {
            std::lock_guard<std::mutex> lk(st.stats_mtx);
            Stats& s = st.stats;
            s.inferences++;
            s.infer_ms_last = dt;
            s.infer_ms_avg = (s.infer_ms_avg == 0.0) ? dt : 0.9 * s.infer_ms_avg + 0.1 * dt;
            s.persons = static_cast<int>(n);
            s.motion_frac = motion;
            if (n > 0 && s.first_person_uptime < 0) {
                s.first_person_uptime = up;
                first = true;
                app_start = s.app_start_uptime;
            }
        }
        if (first)
            PLOG("FIRST_PERSON_DETECTED uptime=%.2f s (%.2f s after app start)", up, up - app_start);

        if (csv) {
            std::fprintf(csv, "%.3f,%.1f,%zu,%.4f\n", up, dt, n, motion);
            if (++csv_lines % 10 == 0) std::fflush(csv);
        }
    }
    if (csv) std::fclose(csv);
}

void printUsage() {
    std::printf(
        "picam-detector — realtime person detection camera\n"
        "Configured with PICAM_* environment variables (see /etc/default/picam).\n"
        "  --print-config   print the effective configuration and exit\n"
        "  --help           this text\n");
}

}  // namespace

int main(int argc, char** argv) {
    bool print_only = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            printUsage();
            return 0;
        }
        if (!std::strcmp(argv[i], "--print-config")) print_only = true;
    }

    // Intentionally never freed: detached HTTP client threads may still use them at exit.
    auto* cfg = new Config(Config::fromEnv());
    auto* st = new SharedState();
    g_state = st;

    if (print_only) {
        cfg->print();
        return 0;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGPIPE, SIG_IGN);

    const double start_up = util::uptimeSeconds();
    {
        std::lock_guard<std::mutex> lk(st->stats_mtx);
        st->stats.app_start_uptime = start_up;
    }
    PLOG("START uptime=%.2f s", start_up);
    cfg->print();

    int threads = cfg->threads;
    if (threads <= 0) threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads <= 0) threads = 1;
    cv::setNumThreads(threads);
    PLOG("OpenCV %s, %d threads", CV_VERSION, threads);

    // Open the camera while the model loads: both are slow, doing them in
    // parallel shortens the time from boot to the first detection.
    std::thread cam_thread(cameraLoop, std::cref(*cfg), std::ref(*st));
    std::thread http_thread(httpServerLoop, std::cref(*cfg), std::ref(*st));

    std::unique_ptr<PersonDetector> det;
    try {
        const double t0 = util::nowMs();
        det = std::make_unique<PersonDetector>(cfg->model_path, cfg->input_size, cfg->class_id,
                                               cfg->conf_threshold, cfg->nms_threshold);
        cv::Mat warmup(cfg->cam_height, cfg->cam_width, CV_8UC3, cv::Scalar::all(0));
        det->detect(warmup);  // first forward() allocates buffers; keep it out of the stats
        const double ms = util::nowMs() - t0;
        const double up = util::uptimeSeconds();
        {
            std::lock_guard<std::mutex> lk(st->stats_mtx);
            st->stats.model_load_ms = ms;
            st->stats.ready_uptime = up;
        }
        PLOG("MODEL_READY load+warmup=%.0f ms uptime=%.2f s", ms, up);
    } catch (const std::exception& e) {
        PLOG("FATAL: %s", e.what());
        st->running.store(false);
        st->wakeAll();
        cam_thread.join();
        http_thread.join();
        return 1;
    }

    std::thread infer_thread(inferenceLoop, std::cref(*cfg), std::ref(*st), std::ref(*det));

    std::uint64_t last_frames = st->frameCount();
    std::uint64_t last_infer = 0;
    double last_t = util::nowMs();
    int ticks = 0;
    while (st->running.load()) {
        for (int i = 0; i < 10 && st->running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const double now = util::nowMs();
        const double sec = (now - last_t) / 1000.0;
        const std::uint64_t frames = st->frameCount();
        Stats snap;
        {
            std::lock_guard<std::mutex> lk(st->stats_mtx);
            st->stats.capture_fps = (frames - last_frames) / sec;
            st->stats.infer_fps = (st->stats.inferences - last_infer) / sec;
            last_infer = st->stats.inferences;
            snap = st->stats;
        }
        last_frames = frames;
        last_t = now;

        if (++ticks % 10 == 0)
            PLOG("status: camera %.1f fps | AI %.1f/s, %.0f ms avg | persons %d | skipped %llu | %.1f C | %ld MB free",
                 snap.capture_fps, snap.infer_fps, snap.infer_ms_avg, snap.persons,
                 static_cast<unsigned long long>(snap.skipped_motion), util::cpuTempC(),
                 util::memAvailableMb());
    }

    st->wakeAll();
    infer_thread.join();
    cam_thread.join();
    http_thread.join();
    PLOG("STOP");
    return 0;
}
