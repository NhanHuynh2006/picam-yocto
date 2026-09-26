#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

struct Detection {
    cv::Rect box;
    float score = 0.0f;
};

// Guarded by SharedState::stats_mtx.
struct Stats {
    double app_start_uptime = -1.0;
    double camera_open_uptime = -1.0;
    double ready_uptime = -1.0;
    double first_person_uptime = -1.0;
    double model_load_ms = 0.0;

    double capture_fps = 0.0;
    double infer_fps = 0.0;
    double infer_ms_last = 0.0;
    double infer_ms_avg = 0.0;
    double motion_frac = 0.0;

    int persons = 0;
    bool camera_ok = false;
    std::uint64_t inferences = 0;
    std::uint64_t skipped_motion = 0;
};

// Data shared between the camera, inference and HTTP threads.
// Frames are published as fresh cv::Mat buffers and never written again,
// so readers may hold shallow copies without extra locking.
class SharedState {
public:
    std::atomic<bool> running{true};
    std::atomic<int> stream_clients{0};

    std::mutex stats_mtx;
    Stats stats;

    void publishFrame(cv::Mat&& frame);
    bool waitForNewFrame(std::uint64_t last_seq, cv::Mat& out, std::uint64_t& seq, int timeout_ms);
    std::uint64_t frameCount();

    void publishDetections(std::vector<Detection> dets);

    // Latest frame with boxes drawn, JPEG-encoded. Encoded once per new frame
    // and shared by all HTTP clients. Returns false until the first frame arrives.
    bool annotatedJpeg(std::vector<uchar>& out, int quality);

    std::string statsJson();
    void wakeAll();

private:
    std::mutex frame_mtx_;
    std::condition_variable frame_cv_;
    cv::Mat frame_;
    std::uint64_t frame_seq_ = 0;

    std::mutex det_mtx_;
    std::vector<Detection> dets_;

    std::mutex jpeg_mtx_;
    std::vector<uchar> jpeg_cache_;
    std::uint64_t jpeg_seq_ = 0;
};
