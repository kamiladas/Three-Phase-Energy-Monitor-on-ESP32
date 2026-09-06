#pragma once

#include "energy/measurement_core.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"

namespace em {

// One point per second of measurement time. 240 points cover the complete
// accelerated 24-hour QEMU profile while keeping deterministic RAM usage.
struct HistoryPoint {
    std::uint64_t timestamp_us{};
    std::array<float, kPhaseCount> voltage_v{};
    std::array<float, kPhaseCount> current_a{};
    std::array<float, kPhaseCount> active_power_w{};
    std::array<float, kPhaseCount> apparent_power_va{};
    std::array<float, kPhaseCount> reactive_power_var{};
    std::array<float, kPhaseCount> power_factor{};
    std::array<float, kPhaseCount> frequency_hz{};
    std::array<float, kPhaseCount> imported_energy_wh{};
    std::array<float, kPhaseCount> exported_energy_wh{};
    std::array<std::uint32_t, kPhaseCount> flags{};
    std::uint64_t missing_time_us{};
};

class HistoryStore {
public:
    // Short live cache only. Durable history belongs on SD.
    static constexpr std::size_t kCapacity = 32;
    void publish(const MeasurementSnapshot& snapshot);
    std::size_t copy(std::array<HistoryPoint, kCapacity>& output) const;
    std::size_t size() const;
    bool get(std::size_t chronological_index, HistoryPoint& output) const;

private:
    mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    std::array<HistoryPoint, kCapacity> points_{};
    std::size_t head_{};
    std::size_t size_{};
    std::uint64_t last_stored_us_{};
};

HistoryStore& history_store();

class WaveformStore {
public:
    static constexpr std::size_t kCapacity = 160;
    void publish(const RawFrame* frames, std::size_t count);
    std::size_t size() const;
    bool get(std::size_t chronological_index, RawFrame& output) const;
    std::size_t copy(std::array<RawFrame, kCapacity>& output) const;
private:
    mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    std::array<RawFrame, kCapacity> frames_{};
    std::size_t head_{};
    std::size_t size_{};
};

WaveformStore& waveform_store();

} // namespace em
