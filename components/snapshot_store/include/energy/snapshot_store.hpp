#pragma once

#include "energy/measurement_core.hpp"

#include <atomic>
#include <cstdint>

namespace em {

class SnapshotStore {
public:
    void publish(const MeasurementSnapshot& snapshot);
    bool read(MeasurementSnapshot& snapshot) const;

private:
    mutable std::atomic<std::uint32_t> version_{};
    MeasurementSnapshot snapshot_{};
};

SnapshotStore& snapshot_store();

}  // namespace em
