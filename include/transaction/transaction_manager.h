#pragma once
#include "storage/wal.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace FarhanDB {

enum class TransactionState {
    ACTIVE,
    COMMITTED,
    ABORTED
};

struct Transaction {
    txn_id_t            id;
    TransactionState    state;
    lsn_t               last_lsn;
};

class TransactionManager {
public:
    explicit TransactionManager(WAL* wal);

    Transaction*    Begin();
    bool            Commit(txn_id_t txn_id);
    bool            Abort(txn_id_t txn_id);
    Transaction*    GetTransaction(txn_id_t txn_id);

private:
    WAL*                                        wal_;
    std::atomic<txn_id_t>                       next_txn_id_;
    std::unordered_map<txn_id_t, Transaction>   active_txns_;
    std::mutex                                  mutex_;
};

} // namespace FarhanDB
