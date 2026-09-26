#pragma once

#include <string>

// All settings come from environment variables (PICAM_*), which systemd loads
// from /etc/default/picam. Defaults below are used when a variable is missing.
struct Config {
    std::string camera_device = "/dev/video0";
    int cam_width = 640;
    int cam_height = 480;
    int cam_fps = 15;

    std::string model_path = "/usr/share/picam/model.onnx";
    int input_size = 320;
    int class_id = 0;  // COCO class 0 = person
    float conf_threshold = 0.40f;
    float nms_threshold = 0.45f;
    int threads = 0;  // 0 = all CPU cores

    bool motion_gate = true;
    double motion_threshold = 0.005;  // fraction of changed pixels
    int motion_keepalive_ms = 2000;   // run AI at least this often
    int person_hold_ms = 3000;        // keep running AI this long after a person was seen

    int http_port = 8080;
    int stream_fps = 8;
    int jpeg_quality = 70;

    std::string csv_log;  // empty = disabled

    static Config fromEnv();
    void print() const;
};
