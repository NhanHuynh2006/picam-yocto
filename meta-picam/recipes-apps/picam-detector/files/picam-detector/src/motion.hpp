#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Cheap motion detector: difference of two consecutive downscaled gray frames.
// Costs well under 1 ms per frame, versus hundreds of ms for the neural network,
// so it is used to decide when the network needs to run at all.
class MotionGate {
public:
    // Returns the fraction of pixels that changed since the previous call (0..1).
    double update(const cv::Mat& bgr) {
        cv::Mat small, gray;
        cv::resize(bgr, small, cv::Size(160, 120), 0, 0, cv::INTER_AREA);
        cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
        if (prev_.empty()) {
            prev_ = gray;
            return 1.0;
        }
        cv::Mat diff;
        cv::absdiff(gray, prev_, diff);
        cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);
        prev_ = gray;
        return cv::countNonZero(diff) / static_cast<double>(diff.total());
    }

private:
    cv::Mat prev_;
};
