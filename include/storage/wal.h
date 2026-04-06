#pragma once
#include <cstdint>
#include <string>
#include <fstream>
#include <mutex>

namespace FarhanDB {

using txn_id_t = uint32_t;
using lsn_t    = uint64_t;  // Log Sequence Number

constexpr txn_id_t INVALID_TXN_ID = 0;
constexpr lsn_t    INVALID_LSN    = 0;

enum class LogRecordType : uint8_t {
    BEGIN       = 1,
    COMMIT      = 2,
    ABORT       = 3,
    INSERT      = 4,
    DELETE      = 5,
    UPDATE      = 6,
    CHECKPOINT  = 7
};

struct LogRecord {
    lsn_t           lsn;
    lsn_t           prev_lsn;   // previous LSN for this transaction
    txn_id_t        txn_id;
    LogRecordType   type;
    uint32_t        page_id;
    uint16_t        slot_id;
    uint32_t        data_length;
    // followed by: old_data[data_length], new_data[data_length]
};

class WAL {
public:
    explicit WAL(const std::string& log_file);
    ~WAL();

    lsn_t   AppendBegin(txn_id_t txn_id);
    lsn_t   AppendCommit(txn_id_t txn_id, lsn_t prev_lsn);
    lsn_t   AppendAbort(txn_id_t txn_id, lsn_t prev_lsn);
    lsn_t   AppendInsert(txn_id_t txn_id, lsn_t prev_lsn,
                         uint32_t page_id, uint16_t slot_id,
                         const char* data, uint32_t length);
    lsn_t   AppendDelete(txn_id_t txn_id, lsn_t prev_lsn,
                         uint32_t page_id, uint16_t slot_id,
                         const char* old_data, uint32_t length);

    void    Flush();
    lsn_t   GetCurrentLSN() const { return current_lsn_; }

private:
    std::string     log_file_;
    std::ofstream   log_stream_;
    lsn_t           current_lsn_;
    std::mutex      mutex_;

    lsn_t   NextLSN();
};

} // namespace FarhanDB
