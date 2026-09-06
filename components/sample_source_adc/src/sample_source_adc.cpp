#include "energy/sample_source_adc.hpp"

#include "energy/board_config.h"

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/adc_types.h"
#include "soc/soc_caps.h"

#include <array>

namespace em {
namespace {

constexpr char kTag[] = "adc_source";
constexpr std::size_t kChannelCount = 6;
constexpr std::size_t kDmaReadBytes = 768;
constexpr std::size_t kDriverPoolBytes = kDmaReadBytes * 4;
constexpr std::uint32_t kAllChannelsMask = (1U << kChannelCount) - 1U;

constexpr std::array<adc_channel_t, kChannelCount> kPattern = {
    static_cast<adc_channel_t>(EM_L1_VOLTAGE_ADC1_CHANNEL),
    static_cast<adc_channel_t>(EM_L1_CURRENT_ADC1_CHANNEL),
    static_cast<adc_channel_t>(EM_L2_VOLTAGE_ADC1_CHANNEL),
    static_cast<adc_channel_t>(EM_L2_CURRENT_ADC1_CHANNEL),
    static_cast<adc_channel_t>(EM_L3_VOLTAGE_ADC1_CHANNEL),
    static_cast<adc_channel_t>(EM_L3_CURRENT_ADC1_CHANNEL),
};

int pattern_index(std::uint8_t channel) {
    for (std::size_t index = 0; index < kPattern.size(); ++index) {
        if (static_cast<std::uint8_t>(kPattern[index]) == channel) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

}  // namespace

AdcSampleSource::AdcSampleSource(float per_channel_sample_rate_hz)
    : per_channel_sample_rate_hz_(per_channel_sample_rate_hz) {}

AdcSampleSource::~AdcSampleSource() {
    stop();
    if (handle_ != nullptr) {
        adc_continuous_deinit(handle_);
        handle_ = nullptr;
    }
}

bool IRAM_ATTR AdcSampleSource::on_pool_overflow(
    adc_continuous_handle_t,
    const adc_continuous_evt_data_t*,
    void* user_data) {
    auto* self = static_cast<AdcSampleSource*>(user_data);
    self->isr_overflows_.fetch_add(1U, std::memory_order_relaxed);
    return false;
}

bool AdcSampleSource::start() {
    if (running_) {
        return true;
    }

    if (handle_ == nullptr) {
        adc_continuous_handle_cfg_t handle_config{};
        handle_config.max_store_buf_size = kDriverPoolBytes;
        handle_config.conv_frame_size = kDmaReadBytes;
        if (adc_continuous_new_handle(&handle_config, &handle_) != ESP_OK) {
            ESP_LOGE(kTag, "Cannot allocate ADC continuous driver");
            return false;
        }

        std::array<adc_digi_pattern_config_t, kChannelCount> adc_pattern{};
        for (std::size_t index = 0; index < adc_pattern.size(); ++index) {
            adc_pattern[index].atten = ADC_ATTEN_DB_12;
            adc_pattern[index].channel = static_cast<std::uint8_t>(kPattern[index]) & 0x7U;
            adc_pattern[index].unit = ADC_UNIT_1;
            adc_pattern[index].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
        }

        adc_continuous_config_t config{};
        config.pattern_num = adc_pattern.size();
        config.adc_pattern = adc_pattern.data();
        config.sample_freq_hz = static_cast<std::uint32_t>(
            per_channel_sample_rate_hz_ * static_cast<float>(kChannelCount));
        config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
        if (adc_continuous_config(handle_, &config) != ESP_OK) {
            ESP_LOGE(kTag, "Cannot configure ADC continuous driver");
            return false;
        }

        adc_continuous_evt_cbs_t callbacks{};
        callbacks.on_pool_ovf = on_pool_overflow;
        if (adc_continuous_register_event_callbacks(handle_, &callbacks, this) != ESP_OK) {
            ESP_LOGE(kTag, "Cannot register ADC callbacks");
            return false;
        }
    }

    pending_mask_ = 0;
    frame_index_ = 0;
    first_timestamp_us_ = static_cast<std::uint64_t>(esp_timer_get_time());
    const esp_err_t result = adc_continuous_start(handle_);
    running_ = result == ESP_OK;
    if (!running_) {
        ESP_LOGE(kTag, "Cannot start ADC: %s", esp_err_to_name(result));
    }
    return running_;
}

bool AdcSampleSource::accept_sample(std::uint8_t channel, std::uint32_t raw, RawFrame& completed) {
    const int index = pattern_index(channel);
    if (index < 0) {
        ++stats_.invalid_samples;
        return false;
    }

    const std::uint32_t bit = 1U << static_cast<unsigned>(index);
    if ((pending_mask_ & bit) != 0U) {
        ++stats_.incomplete_scans;
        pending_mask_ = 0;
    }

    const std::size_t phase = static_cast<std::size_t>(index) / 2U;
    if ((index & 1) == 0) {
        pending_voltage_[phase] = static_cast<float>(raw);
    } else {
        pending_current_[phase] = static_cast<float>(raw);
    }
    pending_mask_ |= bit;

    if (pending_mask_ != kAllChannelsMask) {
        return false;
    }

    completed.voltage = pending_voltage_;
    completed.current = pending_current_;
    completed.timestamp_us = first_timestamp_us_ + static_cast<std::uint64_t>(
        (static_cast<double>(frame_index_) * 1000000.0) / per_channel_sample_rate_hz_);
    ++frame_index_;
    ++stats_.frames_produced;
    pending_mask_ = 0;
    return true;
}

std::size_t AdcSampleSource::read(
    RawFrame* frames,
    std::size_t capacity,
    std::uint32_t timeout_ms) {
    if (!running_ || frames == nullptr || capacity == 0) {
        return 0;
    }

    alignas(4) std::array<std::uint8_t, kDmaReadBytes> raw_buffer{};
    std::uint32_t bytes_read = 0;
    const esp_err_t read_result = adc_continuous_read(
        handle_, raw_buffer.data(), raw_buffer.size(), &bytes_read, timeout_ms);
    if (read_result == ESP_ERR_TIMEOUT) {
        return 0;
    }
    if (read_result != ESP_OK) {
        ++stats_.invalid_samples;
        return 0;
    }

    constexpr std::size_t kMaxParsed = kDmaReadBytes / SOC_ADC_DIGI_RESULT_BYTES;
    std::array<adc_continuous_data_t, kMaxParsed> parsed{};
    std::uint32_t parsed_count = 0;
    if (adc_continuous_parse_data(
            handle_, raw_buffer.data(), bytes_read, parsed.data(), &parsed_count) != ESP_OK) {
        ++stats_.invalid_samples;
        return 0;
    }

    std::size_t produced = 0;
    for (std::uint32_t index = 0; index < parsed_count; ++index) {
        if (!parsed[index].valid || parsed[index].unit != 0) {
            ++stats_.invalid_samples;
            continue;
        }
        RawFrame completed{};
        if (accept_sample(parsed[index].channel, parsed[index].raw_data, completed)) {
            if (produced < capacity) {
                frames[produced++] = completed;
            } else {
                ++stats_.incomplete_scans;
            }
        }
    }
    stats_.dma_pool_overflows = isr_overflows_.load(std::memory_order_relaxed);
    return produced;
}

void AdcSampleSource::stop() {
    if (running_) {
        adc_continuous_stop(handle_);
        running_ = false;
    }
}

AcquisitionStats AdcSampleSource::stats() const {
    AcquisitionStats copy = stats_;
    copy.dma_pool_overflows = isr_overflows_.load(std::memory_order_relaxed);
    return copy;
}

}  // namespace em
