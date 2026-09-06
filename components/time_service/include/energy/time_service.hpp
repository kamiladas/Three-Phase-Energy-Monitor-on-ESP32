#pragma once
#include "esp_err.h"
#include <cstdint>
namespace em {
enum class TimeQuality : std::uint8_t { unknown=0, synced=1, estimated=2 };
struct WallTime { std::int64_t epoch_s{}; TimeQuality quality{TimeQuality::unknown}; };
esp_err_t start_time_service();
WallTime wall_time_now();
const char* time_server();
const char* timezone_rule();
}
