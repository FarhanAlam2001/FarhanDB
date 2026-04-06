#include "transaction/transaction_manager.h"
#include <stdexcept>

namespace FarhanDB {

TransactionManager::TransactionManager(WAL* wal)
    : wal_(wal), next_txn_id_(1) {}

Transaction* TransactionManager::Begin() {
    std::lock_guard<std::mutex> lock(mutex_);
    txn_id_t id = next_txn_id_++;
    Transaction txn;
    txn.id       = id;
    txn.state    = TransactionState::ACTIVE;
    txn.last_lsn = wal_->AppendBegin(id);
    active_txns_[id] = txn;
    return &active_txns_[id];
}

bool TransactionManager::Commit(txn_id_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_txns_.find(txn_id);
    if (it == active_txns_.end()) return false;

    it->second.last_lsn = wal_->AppendCommit(txn_id, it->second.last_lsn);
    it->second.state    = TransactionState::COMMITTED;
    active_txns_.erase(it);
    return true;
}

bool TransactionManager::Abort(txn_id_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_txns_.find(txn_id);
    if (it == active_txns_.end()) return false;

    it->second.last_lsn = wal_->AppendAbort(txn_id, it->second.last_lsn);
    it->second.state    = TransactionState::ABORTED;
    active_txns_.erase(it);
    return true;
}

Transaction* TransactionManager::GetTransaction(txn_id_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_txns_.find(txn_id);
    if (it == active_txns_.end()) return nullptr;
    return &it->second;
}

} // namespace FarhanDB
