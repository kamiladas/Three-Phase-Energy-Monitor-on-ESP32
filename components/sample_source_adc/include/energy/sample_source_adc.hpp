#pragma once

#include "energy/sample_source.hpp"

#include "esp_adc/adc_continuous.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace em {

class AdcSampleSource final : public SampleSource {
public:
    explicit AdcSampleSource(float per_channel_sample_rate_hz = 4000.0F);
    ~AdcSampleSource() override;

    bool start() override;
    std::size_t read(RawFrame* frames, std::size_t capacity, std::uint32_t timeout_ms) override;
    void stop() override;
    AcquisitionStats stats() const override;

private:
    static bool on_pool_overflow(
        adc_continuous_handle_t handle,
        const adc_continuous_evt_data_t* event,
        void* user_data);
    bool accept_sample(std::uint8_t channel, std::uint32_t raw, RawFrame& completed);

    adc_continuous_handle_t handle_{};
    float per_channel_sample_rate_hz_{};
    std::array<float, kPhaseCount> pending_voltage_{};
    std::array<float, kPhaseCount> pending_current_{};
    std::uint32_t pending_mask_{};
    std::uint64_t first_timestamp_us_{};
    std::uint64_t frame_index_{};
    AcquisitionStats stats_{};
    std::atomic<std::uint32_t> isr_overflows_{};
    bool running_{};
};

}  // namespace em
