#include "energy/measurement_core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace em {
namespace {

constexpr double kSecondsPerHour = 3600.0;

double clamp_pf(double value) {
    return std::max(-1.0, std::min(1.0, value));
}

}  // namespace

EngineConfig default_engine_config() {
    EngineConfig config;
    // Starting calibration values are retained only as commissioning defaults.
    // They must later be verified against the production PCB and reference meter.
    config.phase[0] = {{0.9888135593F, 1945.0F}, {0.00508410746F, 1947.0F}};
    config.phase[1] = {{1.03125F, 1945.0F}, {0.00508410746F, 1955.0F}};
    config.phase[2] = {{0.9588135593F, 1947.0F}, {0.00508410746F, 1947.0F}};
    return config;
}

MeasurementEngine::MeasurementEngine(const EngineConfig& config) : config_(config) {
    const double samples_per_window =
        static_cast<double>(config_.sample_rate_hz) * config_.window_cycles /
        config_.nominal_frequency_hz;
    target_samples_ = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(samples_per_window)));

    const double dt = 1.0 / std::max(1.0F, config_.sample_rate_hz);
    const double tau = std::max(static_cast<double>(config_.offset_time_constant_s), dt);
    offset_alpha_ = 1.0 - std::exp(-dt / tau);
    reset();
}

void MeasurementEngine::reset() {
    state_ = {};
    for (std::size_t phase = 0; phase < kPhaseCount; ++phase) {
        state_[phase].voltage_offset = config_.phase[phase].voltage.initial_offset_counts;
        state_[phase].current_offset = config_.phase[phase].current.initial_offset_counts;
    }
    sample_count_ = 0;
    window_start_us_ = 0;
    last_timestamp_us_ = 0;
    sequence_ = 0;
    valid_elapsed_us_ = 0;
    missing_time_us_ = 0;
}

void MeasurementEngine::clear_window() {
    for (auto& phase : state_) {
        phase.sum_v2 = 0.0;
        phase.sum_i2 = 0.0;
        phase.sum_vi = 0.0;
        phase.crossing_period_sum_us = 0.0;
        phase.crossing_period_count = 0;
        phase.flags = 0;
    }
    sample_count_ = 0;
    window_start_us_ = 0;
    valid_elapsed_us_ = 0;
    missing_time_us_ = 0;
}

bool MeasurementEngine::push(const RawFrame& frame, MeasurementSnapshot& output) {
    if (sample_count_ == 0) {
        window_start_us_ = frame.timestamp_us;
    }

    if (last_timestamp_us_ != 0) {
        const std::uint64_t expected_period_us = static_cast<std::uint64_t>(
            std::llround(1000000.0 / config_.sample_rate_hz));
        if (frame.timestamp_us <= last_timestamp_us_) {
            for (auto& phase : state_) {
                phase.flags |= kIncompleteWindow;
            }
        } else {
            const std::uint64_t delta_us = frame.timestamp_us - last_timestamp_us_;
            const std::uint64_t gap_limit_us = expected_period_us + expected_period_us / 2U;
            if (delta_us > gap_limit_us) {
                missing_time_us_ += delta_us - expected_period_us;
                for (auto& phase : state_) {
                    phase.flags |= kSampleGap;
                    // Do not interpret the first zero crossing after an outage
                    // as one extremely long mains period.
                    phase.has_crossing = false;
                    phase.crossing_armed = false;
                }
            } else {
                valid_elapsed_us_ += delta_us;
            }
        }
    }
    last_timestamp_us_ = frame.timestamp_us;

    for (std::size_t phase = 0; phase < kPhaseCount; ++phase) {
        auto& s = state_[phase];
        const auto& calibration = config_.phase[phase];
        const double raw_v = frame.voltage[phase];
        const double raw_i = frame.current[phase];

        if (!std::isfinite(raw_v) || !std::isfinite(raw_i)) {
            s.flags |= kNonFiniteInput;
            continue;
        }

        const double low = config_.adc_min_count + config_.clipping_margin_count;
        const double high = config_.adc_max_count - config_.clipping_margin_count;
        if (raw_v <= low || raw_v >= high || raw_i <= low || raw_i >= high) {
            s.flags |= kAdcClipping;
        }

        // DC tracker: tau is much longer than a mains period, so it follows bias
        // drift while negligibly attenuating the wanted 50/60 Hz waveform.
        s.voltage_offset += offset_alpha_ * (raw_v - s.voltage_offset);
        s.current_offset += offset_alpha_ * (raw_i - s.current_offset);

        const double voltage = (raw_v - s.voltage_offset) * calibration.voltage.scale_per_count;
        const double current = (raw_i - s.current_offset) * calibration.current.scale_per_count;
        s.sum_v2 += voltage * voltage;
        s.sum_i2 += current * current;
        s.sum_vi += voltage * current;

        const double hysteresis = config_.zero_cross_hysteresis_v;
        if (voltage < -hysteresis) {
            s.crossing_armed = true;
        } else if (s.crossing_armed && voltage >= 0.0 && s.last_voltage < 0.0) {
            const double denominator = voltage - s.last_voltage;
            const double fraction = denominator != 0.0 ? -s.last_voltage / denominator : 0.0;
            const double crossing_us = static_cast<double>(frame.timestamp_us) -
                (1.0 - fraction) * (1000000.0 / config_.sample_rate_hz);
            if (s.has_crossing) {
                s.crossing_period_sum_us += crossing_us - s.last_crossing_us;
                ++s.crossing_period_count;
            }
            s.last_crossing_us = crossing_us;
            s.has_crossing = true;
            s.crossing_armed = false;
        }
        s.last_voltage = voltage;
    }

    ++sample_count_;
    if (sample_count_ < target_samples_) {
        return false;
    }

    complete(output);
    clear_window();
    return true;
}

void MeasurementEngine::complete(MeasurementSnapshot& output) {
    output = {};
    output.sequence = ++sequence_;
    output.started_at_us = window_start_us_;
    output.ended_at_us = last_timestamp_us_;
    output.sample_count = sample_count_;
    output.missing_time_us = missing_time_us_;

    // A detected gap never contributes energy. Only intervals backed by actual
    // consecutive samples are integrated.
    const double duration_s = static_cast<double>(valid_elapsed_us_) / 1000000.0;

    for (std::size_t phase = 0; phase < kPhaseCount; ++phase) {
        auto& s = state_[phase];
        auto& result = output.phase[phase];
        const double count = static_cast<double>(sample_count_);
        const double vrms = std::sqrt(s.sum_v2 / count);
        const double irms = std::sqrt(s.sum_i2 / count);
        const double active = s.sum_vi / count;
        const double apparent = vrms * irms;
        const double pf = apparent > std::numeric_limits<double>::epsilon() ? active / apparent : 0.0;
        const double reactive_squared = std::max(0.0, apparent * apparent - active * active);
        const double reactive = std::copysign(std::sqrt(reactive_squared), pf);

        if (active >= 0.0) {
            s.imported_wh += active * duration_s / kSecondsPerHour;
        } else {
            s.exported_wh += -active * duration_s / kSecondsPerHour;
        }

        result.voltage_rms_v = static_cast<float>(vrms);
        result.current_rms_a = static_cast<float>(irms);
        result.active_power_w = static_cast<float>(active);
        result.apparent_power_va = static_cast<float>(apparent);
        result.reactive_power_var = static_cast<float>(reactive);
        result.power_factor = static_cast<float>(clamp_pf(pf));
        result.voltage_offset_counts = static_cast<float>(s.voltage_offset);
        result.current_offset_counts = static_cast<float>(s.current_offset);
        result.imported_energy_wh = s.imported_wh;
        result.exported_energy_wh = s.exported_wh;
        result.quality_flags = s.flags;

        if (s.crossing_period_count > 0 && s.crossing_period_sum_us > 0.0) {
            const double average_period_us = s.crossing_period_sum_us / s.crossing_period_count;
            result.frequency_hz = static_cast<float>(1000000.0 / average_period_us);
        } else {
            result.frequency_hz = 0.0F;
            result.quality_flags |= kFrequencyInvalid;
        }
    }
}

}  // namespace em
