#pragma once

#include "energy/measurement_core.hpp"

#include <cstddef>
#include <cstdint>

namespace em {

struct AcquisitionStats {
    std::uint64_t frames_produced{};
    std::uint64_t invalid_samples{};
    std::uint64_t dma_pool_overflows{};
    std::uint64_t incomplete_scans{};
};

class SampleSource {
public:
    virtual ~SampleSource() = default;
    virtual bool start() = 0;
    virtual std::size_t read(RawFrame* frames, std::size_t capacity, std::uint32_t timeout_ms) = 0;
    virtual void stop() = 0;
    virtual AcquisitionStats stats() const = 0;
};

}  // namespace em
