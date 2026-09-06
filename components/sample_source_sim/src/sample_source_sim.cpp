#include "energy/sample_source_sim.hpp"

#include <algorithm>
#include <cmath>

namespace em {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSqrt2 = 1.41421356237309504880;

float bounded_pf(float value) {
    return std::max(-1.0F, std::min(1.0F, value));
}

float smooth_pulse(double hour, double start, double end, double edge = 0.18) {
    const auto sigmoid = [](double value) { return 1.0 / (1.0 + std::exp(-value)); };
    return static_cast<float>(sigmoid((hour - start) / edge) - sigmoid((hour - end) / edge));
}

void household_operating_point(const SimulatorConfig& config, std::size_t phase,
                               double signal_time_s, float& voltage, float& current, float& pf) {
    if (config.household_day_duration_s <= 0.0F) {
        return;
    }
    const double hour = std::fmod(signal_time_s / config.household_day_duration_s * 24.0, 24.0);
    const double daily = 2.0 * kPi * hour / 24.0;
    static constexpr float base_a[] = {0.08F, 0.04F, 0.12F};
    current = base_a[phase];
    if (phase == 0) {
        current += 2.8F * smooth_pulse(hour, 6.4, 8.2) + 4.6F * smooth_pulse(hour, 17.2, 21.8);
        current += 6.5F * smooth_pulse(hour, 18.35, 18.85, 0.035); // kettle/oven
        pf = current < 0.25F ? 0.55F : 0.94F;
    } else if (phase == 1) {
        current += 0.9F * smooth_pulse(hour, 7.0, 15.5) + 3.4F * smooth_pulse(hour, 11.7, 13.2);
        current += 5.2F * smooth_pulse(hour, 19.0, 20.1, 0.06); // washing machine heater
        pf = current < 0.20F ? 0.42F : 0.86F;
    } else {
        current += 0.55F * smooth_pulse(hour, 0.0, 5.2) + 1.7F * smooth_pulse(hour, 8.0, 16.0);
        current += 7.0F * smooth_pulse(hour, 21.2, 22.0, 0.04); // EV/tool load
        // A deliberate near-zero interval demonstrates that phases are independent.
        current *= 1.0F - 0.97F * smooth_pulse(hour, 13.8, 15.0, 0.04);
        pf = current < 0.20F ? 0.38F : 0.78F;
    }
    current = std::max(0.0F, current * static_cast<float>(1.0 + 0.06 * std::sin(daily * 7.0 + phase)));
    voltage = 230.0F + static_cast<float>(3.2 * std::sin(daily - 0.7 * phase)) - 0.22F * current;
}

}  // namespace

SimulatedSampleSource::SimulatedSampleSource(
    const SimulatorConfig& simulator,
    const EngineConfig& engine)
    : simulator_(simulator), engine_(engine) {}

void SimulatedSampleSource::reset() {
    sample_index_ = 0;
    time_offset_us_ = 0;
    random_state_ = 0x4D595DF4U;
    stats_ = {};
}

bool SimulatedSampleSource::start() {
    reset();
    running_ = true;
    return true;
}

std::size_t SimulatedSampleSource::read(
    RawFrame* frames,
    std::size_t capacity,
    std::uint32_t) {
    if (!running_ || frames == nullptr) {
        return 0;
    }
    for (std::size_t index = 0; index < capacity; ++index) {
        frames[index] = next();
    }
    stats_.frames_produced += capacity;
    return capacity;
}

void SimulatedSampleSource::stop() {
    running_ = false;
}

AcquisitionStats SimulatedSampleSource::stats() const {
    return stats_;
}

void SimulatedSampleSource::simulate_power_loss(std::uint64_t duration_us) {
    time_offset_us_ += duration_us;
}

float SimulatedSampleSource::noise() {
    // Sum of uniforms gives deterministic, approximately Gaussian noise.
    double sum = 0.0;
    for (int i = 0; i < 12; ++i) {
        random_state_ ^= random_state_ << 13;
        random_state_ ^= random_state_ >> 17;
        random_state_ ^= random_state_ << 5;
        sum += static_cast<double>(random_state_) / 4294967295.0;
    }
    return static_cast<float>((sum - 6.0) * simulator_.noise_rms_counts);
}

RawFrame SimulatedSampleSource::next() {
    RawFrame frame;
    const double time_s = static_cast<double>(sample_index_) / simulator_.sample_rate_hz;
    frame.timestamp_us = time_offset_us_ + static_cast<std::uint64_t>(
        std::llround(time_s * 1000000.0));

    for (std::size_t phase = 0; phase < kPhaseCount; ++phase) {
        const auto& signal = simulator_.phase[phase];
        float voltage_rms = signal.voltage_rms_v;
        float current_rms = signal.current_rms_a;
        float configured_pf = signal.power_factor;
        household_operating_point(simulator_, phase, time_s, voltage_rms, current_rms, configured_pf);
        const double phase_angle = (2.0 * kPi * static_cast<double>(phase)) / 3.0;
        const double voltage_angle = 2.0 * kPi * simulator_.frequency_hz * time_s - phase_angle;
        const double pf = bounded_pf(configured_pf);
        double displacement = std::acos(std::abs(pf));
        if (signal.current_leads_voltage) {
            displacement = -displacement;
        }
        if (pf < 0.0) {
            displacement += kPi;
        }

        const double voltage = signal.enabled
            ? voltage_rms * kSqrt2 * std::sin(voltage_angle)
            : 0.0;
        const double current = signal.enabled
            ? current_rms * kSqrt2 * std::sin(voltage_angle - displacement)
            : 0.0;
        const double voltage_offset = simulator_.voltage_offset_counts[phase] +
            simulator_.voltage_drift_counts_per_s[phase] * time_s;
        const double current_offset = simulator_.current_offset_counts[phase] +
            simulator_.current_drift_counts_per_s[phase] * time_s;

        double raw_v = voltage_offset + voltage / engine_.phase[phase].voltage.scale_per_count + noise();
        double raw_i = current_offset + current / engine_.phase[phase].current.scale_per_count + noise();
        if (simulator_.clamp_to_adc_range) {
            raw_v = std::max<double>(engine_.adc_min_count, std::min<double>(engine_.adc_max_count, raw_v));
            raw_i = std::max<double>(engine_.adc_min_count, std::min<double>(engine_.adc_max_count, raw_i));
        }
        frame.voltage[phase] = static_cast<float>(raw_v);
        frame.current[phase] = static_cast<float>(raw_i);
    }

    ++sample_index_;
    return frame;
}

}  // namespace em
