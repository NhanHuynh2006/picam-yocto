#include "config.hpp"

#include <cstdlib>
#include <string>

#include "util.hpp"

namespace {

const char* rawEnv(const char* key) {
    const char* v = std::getenv(key);
    return (v != nullptr && *v != '\0') ? v : nullptr;
}

std::string envStr(const char* key, const std::string& def) {
    const char* v = rawEnv(key);
    return v ? std::string(v) : def;
}

int envInt(const char* key, int def) {
    const char* v = rawEnv(key);
    if (!v) return def;
    char* end = nullptr;
    long x = std::strtol(v, &end, 10);
    if (end == v || *end != '\0') {
        PLOG("WARN: %s='%s' is not an integer, using %d", key, v, def);
        return def;
    }
    return static_cast<int>(x);
}

double envDouble(const char* key, double def) {
    const char* v = rawEnv(key);
    if (!v) return def;
    char* end = nullptr;
    double x = std::strtod(v, &end);
    if (end == v || *end != '\0') {
        PLOG("WARN: %s='%s' is not a number, using %g", key, v, def);
        return def;
    }
    return x;
}

bool envBool(const char* key, bool def) {
    const char* v = rawEnv(key);
    if (!v) return def;
    std::string s(v);
    if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return false;
    PLOG("WARN: %s='%s' is not a boolean, using %d", key, v, def ? 1 : 0);
    return def;
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

Config Config::fromEnv() {
    Config c;
    c.camera_device = envStr("PICAM_CAMERA", c.camera_device);
    c.cam_width = clampInt(envInt("PICAM_CAM_WIDTH", c.cam_width), 160, 1920);
    c.cam_height = clampInt(envInt("PICAM_CAM_HEIGHT", c.cam_height), 120, 1080);
    c.cam_fps = clampInt(envInt("PICAM_CAM_FPS", c.cam_fps), 1, 60);

    c.model_path = envStr("PICAM_MODEL", c.model_path);
    c.input_size = clampInt(envInt("PICAM_INPUT_SIZE", c.input_size), 64, 1280);
    c.class_id = clampInt(envInt("PICAM_CLASS_ID", c.class_id), 0, 1000);
    c.conf_threshold = static_cast<float>(envDouble("PICAM_CONF", c.conf_threshold));
    c.nms_threshold = static_cast<float>(envDouble("PICAM_NMS", c.nms_threshold));
    c.threads = clampInt(envInt("PICAM_THREADS", c.threads), 0, 64);

    c.motion_gate = envBool("PICAM_MOTION_GATE", c.motion_gate);
    c.motion_threshold = envDouble("PICAM_MOTION_THRESHOLD", c.motion_threshold);
    c.motion_keepalive_ms = clampInt(envInt("PICAM_MOTION_KEEPALIVE_MS", c.motion_keepalive_ms), 100, 600000);
    c.person_hold_ms = clampInt(envInt("PICAM_PERSON_HOLD_MS", c.person_hold_ms), 0, 600000);

    c.http_port = clampInt(envInt("PICAM_HTTP_PORT", c.http_port), 1, 65535);
    c.stream_fps = clampInt(envInt("PICAM_STREAM_FPS", c.stream_fps), 1, 30);
    c.jpeg_quality = clampInt(envInt("PICAM_JPEG_QUALITY", c.jpeg_quality), 10, 100);

    c.csv_log = envStr("PICAM_CSV_LOG", c.csv_log);
    return c;
}

void Config::print() const {
    PLOG("config: camera=%s %dx%d@%d model=%s input=%d class=%d conf=%.2f nms=%.2f threads=%d",
         camera_device.c_str(), cam_width, cam_height, cam_fps, model_path.c_str(), input_size,
         class_id, conf_threshold, nms_threshold, threads);
    PLOG("config: motion_gate=%d threshold=%.4f keepalive=%dms hold=%dms http=%d stream_fps=%d jpeg=%d csv=%s",
         motion_gate ? 1 : 0, motion_threshold, motion_keepalive_ms, person_hold_ms, http_port,
         stream_fps, jpeg_quality, csv_log.empty() ? "(off)" : csv_log.c_str());
}
