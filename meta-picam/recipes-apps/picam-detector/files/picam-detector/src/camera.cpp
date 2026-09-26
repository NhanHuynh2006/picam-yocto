#include "camera.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>

#include <opencv2/videoio.hpp>

#include "util.hpp"

namespace {

bool isNumber(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

bool openCamera(cv::VideoCapture& cap, const Config& cfg) {
    const bool ok = isNumber(cfg.camera_device)
                        ? cap.open(std::stoi(cfg.camera_device), cv::CAP_V4L2)
                        : cap.open(cfg.camera_device, cv::CAP_V4L2);
    if (!ok || !cap.isOpened()) return false;

    // MJPG lets cheap webcams deliver full frame rate over USB 2.0.
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, cfg.cam_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, cfg.cam_height);
    cap.set(cv::CAP_PROP_FPS, cfg.cam_fps);
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);  // always the newest frame, no backlog
    return true;
}

void sleepWhileRunning(SharedState& st, int ms) {
    for (int waited = 0; waited < ms && st.running.load(); waited += 100)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}  // namespace

void cameraLoop(const Config& cfg, SharedState& st) {
    int open_attempts = 0;
    while (st.running.load()) {
        cv::VideoCapture cap;
        if (!openCamera(cap, cfg)) {
            if (open_attempts++ % 10 == 0)
                PLOG("camera %s not available yet, retrying...", cfg.camera_device.c_str());
            sleepWhileRunning(st, 1000);
            continue;
        }
        open_attempts = 0;

        const double up = util::uptimeSeconds();
        PLOG("CAMERA_OPENED %s %dx%d @ %.0f fps uptime=%.2f s", cfg.camera_device.c_str(),
             static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
             static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)), cap.get(cv::CAP_PROP_FPS), up);
        {
            std::lock_guard<std::mutex> lk(st.stats_mtx);
            st.stats.camera_ok = true;
            if (st.stats.camera_open_uptime < 0) st.stats.camera_open_uptime = up;
        }

        int failures = 0;
        while (st.running.load()) {
            cv::Mat frame;  // new buffer every frame: readers keep shallow copies safely
            if (!cap.read(frame) || frame.empty()) {
                if (++failures > 20) {
                    PLOG("camera stopped delivering frames, reopening");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            failures = 0;
            st.publishFrame(std::move(frame));
        }

        std::lock_guard<std::mutex> lk(st.stats_mtx);
        st.stats.camera_ok = false;
    }
}
