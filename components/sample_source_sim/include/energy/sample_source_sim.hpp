#pragma once

#include "energy/sample_source.hpp"

#include <array>
#include <cstdint>

namespace em {

struct SimulatedPhase {
    float voltage_rms_v{230.0F};
    float current_rms_a{5.0F};
    float power_factor{1.0F};
    bool current_leads_voltage{false};
    bool enabled{true};
};

struct SimulatorConfig {
    std::array<SimulatedPhase, kPhaseCount> phase{};
    float frequency_hz{50.0F};
    float sample_rate_hz{4000.0F};
    std::array<float, kPhaseCount> voltage_offset_counts{1945.0F, 1945.0F, 1947.0F};
    std::array<float, kPhaseCount> current_offset_counts{1947.0F, 1955.0F, 1947.0F};
    std::array<float, kPhaseCount> voltage_drift_counts_per_s{};
    std::array<float, kPhaseCount> current_drift_counts_per_s{};
    float noise_rms_counts{0.0F};
    // Zero keeps a constant load. A positive value maps that many seconds of
    // emulator runtime onto a complete, repeating 24-hour household profile.
    float household_day_duration_s{0.0F};
    bool clamp_to_adc_range{true};
};

class SimulatedSampleSource final : public SampleSource {
public:
    SimulatedSampleSource(const SimulatorConfig& simulator, const EngineConfig& engine);
    RawFrame next();
    void reset();
    bool start() override;
    std::size_t read(RawFrame* frames, std::size_t capacity, std::uint32_t timeout_ms) override;
    void stop() override;
    AcquisitionStats stats() const override;
    void simulate_power_loss(std::uint64_t duration_us);

private:
    float noise();

    SimulatorConfig simulator_{};
    EngineConfig engine_{};
    std::uint64_t sample_index_{};
    std::uint64_t time_offset_us_{};
    std::uint32_t random_state_{0x4D595DF4U};
    AcquisitionStats stats_{};
    bool running_{};
};

}  // namespace em
