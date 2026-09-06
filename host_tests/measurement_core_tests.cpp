#include "energy/measurement_core.hpp"
#include "energy/sample_source_sim.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require_near(const std::string& name, double actual, double expected, double tolerance) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << name << ": expected " << expected << " +/- " << tolerance
                  << ", got " << actual << '\n';
        std::exit(1);
    }
}

void require_true(const std::string& name, bool condition) {
    if (!condition) {
        std::cerr << name << ": condition failed\n";
        std::exit(1);
    }
}

em::MeasurementSnapshot run_windows(
    em::MeasurementEngine& engine,
    em::SimulatedSampleSource& source,
    int window_count) {
    em::MeasurementSnapshot snapshot;
    int completed = 0;
    while (completed < window_count) {
        if (engine.push(source.next(), snapshot)) {
            ++completed;
        }
    }
    return snapshot;
}

void test_balanced_three_phase() {
    auto engine_config = em::default_engine_config();
    em::SimulatorConfig simulator_config;
    for (auto& phase : simulator_config.phase) {
        phase.voltage_rms_v = 230.0F;
        phase.current_rms_a = 5.0F;
        phase.power_factor = 0.8F;
    }

    em::MeasurementEngine engine(engine_config);
    em::SimulatedSampleSource source(simulator_config, engine_config);
    const auto result = run_windows(engine, source, 5);

    for (std::size_t phase = 0; phase < em::kPhaseCount; ++phase) {
        const auto& p = result.phase[phase];
        require_near("voltage RMS", p.voltage_rms_v, 230.0, 0.8);
        require_near("current RMS", p.current_rms_a, 5.0, 0.03);
        require_near("active power", p.active_power_w, 920.0, 4.0);
        require_near("apparent power", p.apparent_power_va, 1150.0, 5.0);
        require_near("power factor", p.power_factor, 0.8, 0.01);
        require_near("frequency", p.frequency_hz, 50.0, 0.1);
        require_true("no quality errors", p.quality_flags == em::kQualityOk);
        require_true("imported energy grows", p.imported_energy_wh > 0.0);
        require_near("exported energy zero", p.exported_energy_wh, 0.0, 1e-9);
    }
}

void test_dynamic_offset_drift() {
    auto engine_config = em::default_engine_config();
    engine_config.offset_time_constant_s = 1.0F;
    em::SimulatorConfig simulator_config;
    for (std::size_t phase = 0; phase < em::kPhaseCount; ++phase) {
        simulator_config.phase[phase].voltage_rms_v = 230.0F;
        simulator_config.phase[phase].current_rms_a = 2.0F;
        simulator_config.phase[phase].power_factor = 1.0F;
        simulator_config.voltage_drift_counts_per_s[phase] = 3.0F;
        simulator_config.current_drift_counts_per_s[phase] = -2.0F;
    }

    em::MeasurementEngine engine(engine_config);
    em::SimulatedSampleSource source(simulator_config, engine_config);
    const auto result = run_windows(engine, source, 30);

    for (const auto& p : result.phase) {
        require_near("drift voltage RMS", p.voltage_rms_v, 230.0, 1.5);
        require_near("drift current RMS", p.current_rms_a, 2.0, 0.04);
        require_near("drift power factor", p.power_factor, 1.0, 0.01);
    }
}

void test_export_direction() {
    auto engine_config = em::default_engine_config();
    em::SimulatorConfig simulator_config;
    for (auto& phase : simulator_config.phase) {
        phase.current_rms_a = 1.0F;
        phase.power_factor = -1.0F;
    }

    em::MeasurementEngine engine(engine_config);
    em::SimulatedSampleSource source(simulator_config, engine_config);
    const auto result = run_windows(engine, source, 5);

    for (const auto& p : result.phase) {
        require_near("negative active power", p.active_power_w, -230.0, 2.0);
        require_near("signed PF", p.power_factor, -1.0, 0.01);
        require_true("exported energy grows", p.exported_energy_wh > 0.0);
        require_near("imported energy zero", p.imported_energy_wh, 0.0, 1e-9);
    }
}

void test_clipping_is_reported() {
    auto engine_config = em::default_engine_config();
    em::SimulatorConfig simulator_config;
    for (auto& phase : simulator_config.phase) {
        phase.current_rms_a = 20.0F;
    }

    em::MeasurementEngine engine(engine_config);
    em::SimulatedSampleSource source(simulator_config, engine_config);
    const auto result = run_windows(engine, source, 1);
    for (const auto& p : result.phase) {
        require_true("ADC clipping flag", (p.quality_flags & em::kAdcClipping) != 0U);
    }
}

}  // namespace

int main() {
    test_balanced_three_phase();
    test_dynamic_offset_drift();
    test_export_direction();
    test_clipping_is_reported();
    std::cout << "All measurement engine tests passed.\n";
    return 0;
}
