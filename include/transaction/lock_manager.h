#pragma once
#include "storage/wal.h"
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace FarhanDB {

enum class LockMode {
    SHARED,     // Read lock
    EXCLUSIVE   // Write lock
};

struct LockRequest {
    txn_id_t    txn_id;
    LockMode    mode;
    bool        granted;
};

struct LockQueue {
    std::vector<LockRequest>    requests;
    std::mutex                  mutex;
    std::condition_variable     cv;
};

class LockManager {
public:
    // Table-level locks
    bool    AcquireTableLock(txn_id_t txn_id, const std::string& table, LockMode mode);
    bool    ReleaseTableLock(txn_id_t txn_id, const std::string& table);
    void    ReleaseAllLocks(txn_id_t txn_id);

    // Row-level locks (page_id + slot_id)
    bool    AcquireRowLock(txn_id_t txn_id, uint32_t page_id, uint16_t slot_id, LockMode mode);
    bool    ReleaseRowLock(txn_id_t txn_id, uint32_t page_id, uint16_t slot_id);

private:
    using RowKey = std::pair<uint32_t, uint16_t>;

    struct RowKeyHash {
        size_t operator()(const RowKey& k) const {
            return std::hash<uint64_t>()(((uint64_t)k.first << 16) | k.second);
        }
    };

    std::unordered_map<std::string, LockQueue>      table_locks_;
    std::unordered_map<RowKey, LockQueue, RowKeyHash> row_locks_;
    std::mutex                                       global_mutex_;

    bool CanGrant(const LockQueue& queue, LockMode mode, txn_id_t txn_id);
};

} // namespace FarhanDB
