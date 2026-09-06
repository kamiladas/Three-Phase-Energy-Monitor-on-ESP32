#include "energy/snapshot_store.hpp"

namespace em {

void SnapshotStore::publish(const MeasurementSnapshot& snapshot) {
    version_.fetch_add(1U, std::memory_order_acq_rel);
    snapshot_ = snapshot;
    version_.fetch_add(1U, std::memory_order_release);
}

bool SnapshotStore::read(MeasurementSnapshot& snapshot) const {
    for (int attempt = 0; attempt < 4; ++attempt) {
        const auto before = version_.load(std::memory_order_acquire);
        if ((before & 1U) != 0U || before == 0U) {
            continue;
        }
        snapshot = snapshot_;
        const auto after = version_.load(std::memory_order_acquire);
        if (before == after) {
            return true;
        }
    }
    return false;
}

SnapshotStore& snapshot_store() {
    static SnapshotStore instance;
    return instance;
}

}  // namespace em
