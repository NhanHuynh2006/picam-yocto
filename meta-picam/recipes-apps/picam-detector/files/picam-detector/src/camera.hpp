#pragma once

#include "config.hpp"
#include "state.hpp"

// Captures frames from a V4L2 (USB/UVC) camera forever, reopening it if it
// disappears. Each frame is published to SharedState as a fresh buffer.
void cameraLoop(const Config& cfg, SharedState& st);
