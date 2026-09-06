#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace em {

constexpr std::size_t kPhaseCount = 3;

enum QualityFlag : std::uint32_t {
    kQualityOk = 0,
    kIncompleteWindow = 1U << 0,
    kAdcClipping = 1U << 1,
    kFrequencyInvalid = 1U << 2,
    kNonFiniteInput = 1U << 3,
    kSampleGap = 1U << 4,
};

struct RawFrame {
    std::array<float, kPhaseCount> voltage{};
    std::array<float, kPhaseCount> current{};
    std::uint64_t timestamp_us{};
};

struct ChannelCalibration {
    float scale_per_count{1.0F};
    float initial_offset_counts{2048.0F};
};

struct PhaseCalibration {
    ChannelCalibration voltage{};
    ChannelCalibration current{};
};

struct EngineConfig {
    std::array<PhaseCalibration, kPhaseCount> phase{};
    float sample_rate_hz{4000.0F};
    float nominal_frequency_hz{50.0F};
    float window_cycles{10.0F};
    float offset_time_constant_s{2.0F};
    float zero_cross_hysteresis_v{10.0F};
    float adc_min_count{0.0F};
    float adc_max_count{4095.0F};
    float clipping_margin_count{2.0F};
};

struct PhaseResult {
    float voltage_rms_v{};
    float current_rms_a{};
    float active_power_w{};
    float apparent_power_va{};
    float reactive_power_var{};
    float power_factor{};
    float frequency_hz{};
    float voltage_offset_counts{};
    float current_offset_counts{};
    double imported_energy_wh{};
    double exported_energy_wh{};
    std::uint32_t quality_flags{};
};

struct MeasurementSnapshot {
    std::array<PhaseResult, kPhaseCount> phase{};
    std::uint64_t sequence{};
    std::uint64_t started_at_us{};
    std::uint64_t ended_at_us{};
    std::size_t sample_count{};
    std::uint64_t missing_time_us{};
};

class MeasurementEngine {
public:
    explicit MeasurementEngine(const EngineConfig& config);

    // Returns true whenever a complete, immutable measurement window is ready.
    bool push(const RawFrame& frame, MeasurementSnapshot& output);
    void reset();
    const EngineConfig& config() const { return config_; }

private:
    struct PhaseState {
        double voltage_offset{};
        double current_offset{};
        double sum_v2{};
        double sum_i2{};
        double sum_vi{};
        double imported_wh{};
        double exported_wh{};
        double last_voltage{};
        double last_crossing_us{};
        double crossing_period_sum_us{};
        std::size_t crossing_period_count{};
        bool crossing_armed{};
        bool has_crossing{};
        std::uint32_t flags{};
    };

    void clear_window();
    void complete(MeasurementSnapshot& output);

    EngineConfig config_{};
    std::array<PhaseState, kPhaseCount> state_{};
    std::size_t target_samples_{};
    std::size_t sample_count_{};
    std::uint64_t window_start_us_{};
    std::uint64_t last_timestamp_us_{};
    std::uint64_t sequence_{};
    std::uint64_t valid_elapsed_us_{};
    std::uint64_t missing_time_us_{};
    double offset_alpha_{};
};

EngineConfig default_engine_config();

}  // namespace em
