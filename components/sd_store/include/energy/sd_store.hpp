#pragma once
#include "esp_err.h"
#include "energy/measurement_core.hpp"
#include <cstdint>
#include <array>
namespace em {
struct SdStoreStats { bool mounted{}; std::uint64_t written{}; std::uint64_t dropped{}; std::uint64_t write_errors{}; };
struct SdEnergyBucket { bool present{}; std::uint64_t start_us{}; std::int64_t start_epoch_s{}; std::array<double,3> energy_kwh{}; };
struct SdEnergyAggregate { static constexpr std::size_t kMaxBuckets=31; std::array<SdEnergyBucket,kMaxBuckets> buckets{}; std::size_t count{}; std::uint64_t range_us{}; };
struct SdEnergySample { std::int64_t epoch_s{}; std::array<double,3> energy_kwh{}; };
struct SdEnergySeries { static constexpr std::size_t kMaxSamples=256; std::array<SdEnergySample,kMaxSamples> samples{}; std::size_t count{}; };
esp_err_t start_sd_store();
bool sd_store_enqueue(const MeasurementSnapshot& snapshot);
SdStoreStats sd_store_stats();
esp_err_t sd_store_aggregate(std::uint64_t range_us, std::size_t bucket_count, SdEnergyAggregate& output);
esp_err_t sd_store_aggregate_utc(std::int64_t from_epoch_s, std::int64_t to_epoch_s,
                                 std::size_t bucket_count, SdEnergyAggregate& output);
esp_err_t sd_store_energy_series(std::int64_t from_epoch_s, std::int64_t to_epoch_s,
                                 SdEnergySeries& output);
const char* sd_store_csv_path();
}
