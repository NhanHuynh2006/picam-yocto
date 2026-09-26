#pragma once

#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include "state.hpp"

// Runs a YOLOv8-style ONNX detector with OpenCV DNN (CPU) and returns boxes of
// one class. Expected output tensor: [1, 4 + num_classes, N] (or its transpose),
// rows = cx, cy, w, h, class scores. Works for YOLOv8/YOLO11 exported by Ultralytics.
class PersonDetector {
public:
    PersonDetector(const std::string& model_path, int input_size, int class_id, float conf,
                   float nms);

    std::vector<Detection> detect(const cv::Mat& bgr);

private:
    cv::dnn::Net net_;
    int input_;
    int class_id_;
    float conf_;
    float nms_;
};
