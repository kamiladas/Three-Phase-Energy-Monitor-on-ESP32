#include "energy/measurement_core.hpp"
#include "energy/sample_source.hpp"
#include "energy/snapshot_store.hpp"
#include "energy/history_store.hpp"
#include "energy/sd_store.hpp"
#include "energy/web_ui.hpp"
#include "energy/time_service.hpp"
#include "sdkconfig.h"

#if CONFIG_EM_SAMPLE_SOURCE_ADC
#include "energy/sample_source_adc.hpp"
#else
#include "energy/sample_source_sim.hpp"
#endif

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <array>
#include <cinttypes>

namespace {

constexpr char kTag[] = "energy_monitor";
constexpr std::size_t kFramesPerBlock = 40;
constexpr UBaseType_t kQueueDepth = 4;

struct FrameBlock {
    std::array<em::RawFrame, kFramesPerBlock> frames{};
    std::size_t count{};
    std::uint64_t sequence{};
};

QueueHandle_t frame_queue;
em::EngineConfig engine_config = em::default_engine_config();

#if CONFIG_EM_SAMPLE_SOURCE_ADC
em::AdcSampleSource source(engine_config.sample_rate_hz);
#else
em::SimulatorConfig make_simulator_config() {
    em::SimulatorConfig config;
    config.sample_rate_hz = engine_config.sample_rate_hz;
    config.noise_rms_counts = 0.35F;
    // Production-speed simulation: one profile day equals one real day.
    config.household_day_duration_s = 86400.0F;
    for (auto& phase : config.phase) {
        phase.voltage_rms_v = 230.0F;
        phase.current_rms_a = 0.0F;
        phase.power_factor = 0.8F;
    }
    return config;
}
em::SimulatorConfig simulator_config = make_simulator_config();
em::SimulatedSampleSource source(simulator_config, engine_config);
#endif

void sampling_task(void*) {
    if (!source.start()) {
        ESP_LOGE(kTag, "Sample source failed to start");
        vTaskDelete(nullptr);
        return;
    }

    std::uint64_t block_sequence = 0;
    std::uint64_t queue_drops = 0;
#if CONFIG_EM_SAMPLE_SOURCE_SIM
    constexpr TickType_t kSimulatorPeriod = pdMS_TO_TICKS(10);
#endif

    for (;;) {
#if CONFIG_EM_SAMPLE_SOURCE_SIM
        if (block_sequence == 50U) {
            constexpr std::uint64_t kDemoOutageUs = 3000000U;
            ESP_LOGW(kTag, "DEMO: power lost - no samples for 3.000 s");
            source.simulate_power_loss(kDemoOutageUs);
        }
#endif
        FrameBlock block{};
        block.count = source.read(block.frames.data(), block.frames.size(), 100);
        block.sequence = ++block_sequence;
        if (block.count > 0) em::waveform_store().publish(block.frames.data(), block.count);
        if (block.count > 0 && xQueueSend(frame_queue, &block, pdMS_TO_TICKS(2)) != pdPASS) {
            ++queue_drops;
            if ((queue_drops % 100U) == 1U) {
                ESP_LOGW(kTag, "Measurement queue full; dropped=%" PRIu64, queue_drops);
            }
        }
#if CONFIG_EM_SAMPLE_SOURCE_SIM
        // The QEMU CPU can run slower than its emulated tick clock. A relative
        // sleep guarantees scheduler/Idle progress even when a deadline was
        // already missed. This exists only in the simulator backend; hardware
        // acquisition sleeps event-driven inside adc_continuous_read().
        vTaskDelay(kSimulatorPeriod);
#endif
    }
}

void measurement_task(void*) {
    em::MeasurementEngine engine(engine_config);
    FrameBlock block{};
    em::MeasurementSnapshot snapshot{};
    std::uint64_t expected_block = 1;

    for (;;) {
        if (xQueueReceive(frame_queue, &block, portMAX_DELAY) != pdPASS) {
            continue;
        }
        if (block.sequence != expected_block) {
            ESP_LOGW(kTag, "Block discontinuity: expected=%" PRIu64 ", received=%" PRIu64,
                     expected_block, block.sequence);
        }
        expected_block = block.sequence + 1;

        for (std::size_t index = 0; index < block.count; ++index) {
            if (!engine.push(block.frames[index], snapshot)) {
                continue;
            }
            em::snapshot_store().publish(snapshot);
            em::history_store().publish(snapshot);
            em::sd_store_enqueue(snapshot);
            ESP_LOGI(kTag,
                     "window=%" PRIu64 " samples=%u | "
                     "L1 %.2fV %.3fA %.1fW PF %.3f %.2fHz | "
                     "L2 %.2fV %.3fA %.1fW PF %.3f | "
                     "L3 %.2fV %.3fA %.1fW PF %.3f",
                     snapshot.sequence,
                     static_cast<unsigned>(snapshot.sample_count),
                     snapshot.phase[0].voltage_rms_v,
                     snapshot.phase[0].current_rms_a,
                     snapshot.phase[0].active_power_w,
                     snapshot.phase[0].power_factor,
                     snapshot.phase[0].frequency_hz,
                     snapshot.phase[1].voltage_rms_v,
                     snapshot.phase[1].current_rms_a,
                     snapshot.phase[1].active_power_w,
                     snapshot.phase[1].power_factor,
                     snapshot.phase[2].voltage_rms_v,
                     snapshot.phase[2].current_rms_a,
                     snapshot.phase[2].active_power_w,
                     snapshot.phase[2].power_factor);
            if (snapshot.missing_time_us > 0U) {
                ESP_LOGW(kTag,
                         "POWER GAP DETECTED: missing=%.3f s, energy during gap NOT counted, flags=0x%08" PRIx32,
                         static_cast<double>(snapshot.missing_time_us) / 1000000.0,
                         snapshot.phase[0].quality_flags);
            }
        }
    }
}

void network_task(void*) {
    const esp_err_t result = example_connect();
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Network connection failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }
    if (em::start_time_service() != ESP_OK) ESP_LOGW(kTag, "SNTP service failed to start");
    if (em::start_web_ui() != ESP_OK) {
        ESP_LOGE(kTag, "Web UI failed to start");
    } else {
        ESP_LOGI(kTag, "Open dashboard: http://localhost:8000");
    }
    vTaskDelete(nullptr);
}

}  // namespace

extern "C" void app_main() {
    ESP_LOGI(kTag, "Starting non-blocking three-phase measurement pipeline");
#if CONFIG_EM_SAMPLE_SOURCE_ADC
    ESP_LOGI(kTag, "Source: ESP32 ADC1 continuous/DMA, 6 channels at %.0f samples/s total",
             engine_config.sample_rate_hz * 6.0F);
#else
    ESP_LOGI(kTag, "Source: deterministic QEMU simulator at %.0f frames/s",
             engine_config.sample_rate_hz);
#endif

    frame_queue = xQueueCreate(kQueueDepth, sizeof(FrameBlock));
    if (frame_queue == nullptr) {
        ESP_LOGE(kTag, "Cannot allocate frame queue");
        return;
    }

    xTaskCreate(measurement_task, "measurement", 6144, nullptr, 9, nullptr);
    // Create the producer last. A higher-priority task may run immediately at
    // xTaskCreate(), before app_main gets a chance to create its consumer.
    xTaskCreate(sampling_task, "sampling", 4096, nullptr, 8, nullptr);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (em::start_sd_store() != ESP_OK) ESP_LOGW(kTag, "Continuing without SD history");
    xTaskCreate(network_task, "network", 4096, nullptr, 4, nullptr);
}
