#include "energy/history_store.hpp"

namespace em {

void HistoryStore::publish(const MeasurementSnapshot& snapshot) {
    constexpr std::uint64_t kIntervalUs = 1000000;
    if (last_stored_us_ != 0 && snapshot.ended_at_us < last_stored_us_ + kIntervalUs) return;

    HistoryPoint point{};
    point.timestamp_us = snapshot.ended_at_us;
    point.missing_time_us = snapshot.missing_time_us;
    for (std::size_t i = 0; i < kPhaseCount; ++i) {
        const auto& p = snapshot.phase[i];
        point.voltage_v[i] = p.voltage_rms_v;
        point.current_a[i] = p.current_rms_a;
        point.active_power_w[i] = p.active_power_w;
        point.apparent_power_va[i] = p.apparent_power_va;
        point.reactive_power_var[i] = p.reactive_power_var;
        point.power_factor[i] = p.power_factor;
        point.frequency_hz[i] = p.frequency_hz;
        point.imported_energy_wh[i] = static_cast<float>(p.imported_energy_wh);
        point.exported_energy_wh[i] = static_cast<float>(p.exported_energy_wh);
        point.flags[i] = p.quality_flags;
    }
    taskENTER_CRITICAL(&lock_);
    points_[head_] = point;
    head_ = (head_ + 1) % kCapacity;
    if (size_ < kCapacity) ++size_;
    last_stored_us_ = snapshot.ended_at_us;
    taskEXIT_CRITICAL(&lock_);
}

std::size_t HistoryStore::copy(std::array<HistoryPoint, kCapacity>& output) const {
    taskENTER_CRITICAL(&lock_);
    const std::size_t count = size_;
    const std::size_t oldest = (head_ + kCapacity - size_) % kCapacity;
    for (std::size_t i = 0; i < count; ++i) output[i] = points_[(oldest + i) % kCapacity];
    taskEXIT_CRITICAL(&lock_);
    return count;
}

std::size_t HistoryStore::size() const {
    taskENTER_CRITICAL(&lock_);
    const std::size_t result = size_;
    taskEXIT_CRITICAL(&lock_);
    return result;
}

bool HistoryStore::get(std::size_t chronological_index, HistoryPoint& output) const {
    taskENTER_CRITICAL(&lock_);
    if (chronological_index >= size_) {
        taskEXIT_CRITICAL(&lock_);
        return false;
    }
    const std::size_t oldest = (head_ + kCapacity - size_) % kCapacity;
    output = points_[(oldest + chronological_index) % kCapacity];
    taskEXIT_CRITICAL(&lock_);
    return true;
}

HistoryStore& history_store() { static HistoryStore instance; return instance; }

void WaveformStore::publish(const RawFrame* frames, std::size_t count) {
    if (frames == nullptr) return;
    taskENTER_CRITICAL(&lock_);
    for (std::size_t i = 0; i < count; ++i) {
        frames_[head_] = frames[i];
        head_ = (head_ + 1) % kCapacity;
        if (size_ < kCapacity) ++size_;
    }
    taskEXIT_CRITICAL(&lock_);
}

std::size_t WaveformStore::size() const {
    taskENTER_CRITICAL(&lock_); const auto result = size_; taskEXIT_CRITICAL(&lock_); return result;
}

bool WaveformStore::get(std::size_t chronological_index, RawFrame& output) const {
    taskENTER_CRITICAL(&lock_);
    if (chronological_index >= size_) { taskEXIT_CRITICAL(&lock_); return false; }
    const std::size_t oldest = (head_ + kCapacity - size_) % kCapacity;
    output = frames_[(oldest + chronological_index) % kCapacity];
    taskEXIT_CRITICAL(&lock_);
    return true;
}

std::size_t WaveformStore::copy(std::array<RawFrame, kCapacity>& output) const {
    taskENTER_CRITICAL(&lock_);
    const std::size_t count = size_;
    const std::size_t oldest = (head_ + kCapacity - size_) % kCapacity;
    for (std::size_t i = 0; i < count; ++i) output[i] = frames_[(oldest + i) % kCapacity];
    taskEXIT_CRITICAL(&lock_);
    return count;
}

WaveformStore& waveform_store() { static WaveformStore instance; return instance; }

} // namespace em
