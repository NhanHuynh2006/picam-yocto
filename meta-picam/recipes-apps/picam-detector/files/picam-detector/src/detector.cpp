#include "detector.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <opencv2/imgproc.hpp>

PersonDetector::PersonDetector(const std::string& model_path, int input_size, int class_id,
                               float conf, float nms)
    : input_(input_size), class_id_(class_id), conf_(conf), nms_(nms) {
    net_ = cv::dnn::readNetFromONNX(model_path);
    if (net_.empty()) throw std::runtime_error("cannot load model: " + model_path);
    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

std::vector<Detection> PersonDetector::detect(const cv::Mat& bgr) {
    // Letterbox: keep aspect ratio, pad to a square input with gray (114), like Ultralytics.
    const float r = std::min(input_ / static_cast<float>(bgr.cols),
                             input_ / static_cast<float>(bgr.rows));
    const int nw = std::max(1, static_cast<int>(std::lround(bgr.cols * r)));
    const int nh = std::max(1, static_cast<int>(std::lround(bgr.rows * r)));
    const int pad_x = (input_ - nw) / 2;
    const int pad_y = (input_ - nh) / 2;

    cv::Mat canvas(input_, input_, CV_8UC3, cv::Scalar(114, 114, 114));
    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LINEAR);
    resized.copyTo(canvas(cv::Rect(pad_x, pad_y, nw, nh)));

    cv::Mat blob = cv::dnn::blobFromImage(canvas, 1.0 / 255.0, cv::Size(input_, input_),
                                          cv::Scalar(), /*swapRB=*/true, /*crop=*/false);
    net_.setInput(blob);
    cv::Mat out = net_.forward();

    if (out.dims != 3 || out.size[0] != 1)
        throw std::runtime_error("unexpected model output: expected 3-D tensor [1, C, N]");

    const int d1 = out.size[1];
    const int d2 = out.size[2];
    const bool channels_first = d1 < d2;  // YOLOv8 export: [1, 4+nc, N] with N >> 4+nc
    const int num_attr = channels_first ? d1 : d2;
    const int num_boxes = channels_first ? d2 : d1;
    if (num_attr < 5 || 4 + class_id_ >= num_attr)
        throw std::runtime_error("model output has no column for the configured class id");

    cv::Mat raw(d1, d2, CV_32F, out.ptr<float>());
    cv::Mat rows = channels_first ? cv::Mat(raw.t()) : raw;  // -> [N, 4+nc]

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    const float max_x = static_cast<float>(bgr.cols - 1);
    const float max_y = static_cast<float>(bgr.rows - 1);

    for (int i = 0; i < num_boxes; ++i) {
        const float* p = rows.ptr<float>(i);
        const float score = p[4 + class_id_];
        if (score < conf_) continue;

        const float cx = p[0], cy = p[1], w = p[2], h = p[3];
        float x0 = (cx - 0.5f * w - pad_x) / r;
        float y0 = (cy - 0.5f * h - pad_y) / r;
        float x1 = (cx + 0.5f * w - pad_x) / r;
        float y1 = (cy + 0.5f * h - pad_y) / r;
        x0 = std::clamp(x0, 0.0f, max_x);
        y0 = std::clamp(y0, 0.0f, max_y);
        x1 = std::clamp(x1, 0.0f, max_x);
        y1 = std::clamp(y1, 0.0f, max_y);
        if (x1 - x0 < 2.0f || y1 - y0 < 2.0f) continue;

        boxes.emplace_back(cv::Point(static_cast<int>(x0), static_cast<int>(y0)),
                           cv::Point(static_cast<int>(x1), static_cast<int>(y1)));
        scores.push_back(score);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes, scores, conf_, nms_, keep);

    std::vector<Detection> result;
    result.reserve(keep.size());
    for (int idx : keep) result.push_back({boxes[idx], scores[idx]});
    return result;
}
