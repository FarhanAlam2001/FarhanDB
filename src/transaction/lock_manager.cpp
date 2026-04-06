#include "transaction/lock_manager.h"
#include <algorithm>

namespace FarhanDB {

bool LockManager::CanGrant(const LockQueue& queue, LockMode mode, txn_id_t txn_id) {
    for (const auto& req : queue.requests) {
        if (!req.granted) continue;
        if (req.txn_id == txn_id) continue;
        // Shared locks are compatible with each other
        if (mode == LockMode::SHARED && req.mode == LockMode::SHARED) continue;
        return false; // conflict
    }
    return true;
}

bool LockManager::AcquireTableLock(txn_id_t txn_id, const std::string& table, LockMode mode) {
    std::unique_lock<std::mutex> global(global_mutex_);
    auto& queue = table_locks_[table];
    global.unlock();

    std::unique_lock<std::mutex> lock(queue.mutex);
    queue.requests.push_back({txn_id, mode, false});

    queue.cv.wait(lock, [&]() {
        return CanGrant(queue, mode, txn_id);
    });

    for (auto& req : queue.requests) {
        if (req.txn_id == txn_id && !req.granted) {
            req.granted = true;
            break;
        }
    }
    return true;
}

bool LockManager::ReleaseTableLock(txn_id_t txn_id, const std::string& table) {
    std::lock_guard<std::mutex> global(global_mutex_);
    auto it = table_locks_.find(table);
    if (it == table_locks_.end()) return false;

    auto& queue = it->second;
    std::lock_guard<std::mutex> lock(queue.mutex);
    auto& reqs = queue.requests;
    reqs.erase(std::remove_if(reqs.begin(), reqs.end(),
        [txn_id](const LockRequest& r) { return r.txn_id == txn_id; }), reqs.end());
    queue.cv.notify_all();
    return true;
}

void LockManager::ReleaseAllLocks(txn_id_t txn_id) {
    std::lock_guard<std::mutex> global(global_mutex_);
    for (auto& [table, queue] : table_locks_) {
        std::lock_guard<std::mutex> lock(queue.mutex);
        auto& reqs = queue.requests;
        reqs.erase(std::remove_if(reqs.begin(), reqs.end(),
            [txn_id](const LockRequest& r) { return r.txn_id == txn_id; }), reqs.end());
        queue.cv.notify_all();
    }
}

bool LockManager::AcquireRowLock(txn_id_t txn_id, uint32_t page_id,
                                   uint16_t slot_id, LockMode mode) {
    RowKey key{page_id, slot_id};
    std::unique_lock<std::mutex> global(global_mutex_);
    auto& queue = row_locks_[key];
    global.unlock();

    std::unique_lock<std::mutex> lock(queue.mutex);
    queue.requests.push_back({txn_id, mode, false});

    queue.cv.wait(lock, [&]() {
        return CanGrant(queue, mode, txn_id);
    });

    for (auto& req : queue.requests) {
        if (req.txn_id == txn_id && !req.granted) {
            req.granted = true;
            break;
        }
    }
    return true;
}

bool LockManager::ReleaseRowLock(txn_id_t txn_id, uint32_t page_id, uint16_t slot_id) {
    RowKey key{page_id, slot_id};
    std::lock_guard<std::mutex> global(global_mutex_);
    auto it = row_locks_.find(key);
    if (it == row_locks_.end()) return false;

    auto& queue = it->second;
    std::lock_guard<std::mutex> lock(queue.mutex);
    auto& reqs = queue.requests;
    reqs.erase(std::remove_if(reqs.begin(), reqs.end(),
        [txn_id](const LockRequest& r) { return r.txn_id == txn_id; }), reqs.end());
    queue.cv.notify_all();
    return true;
}

} // namespace FarhanDB
