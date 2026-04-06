#include "storage/wal.h"
#include <stdexcept>
#include <cstring>

namespace FarhanDB {

WAL::WAL(const std::string& log_file)
    : log_file_(log_file), current_lsn_(0) {
    log_stream_.open(log_file_, std::ios::out | std::ios::binary | std::ios::app);
    if (!log_stream_.is_open()) {
        throw std::runtime_error("WAL: cannot open log file " + log_file_);
    }
}

WAL::~WAL() {
    Flush();
    if (log_stream_.is_open()) log_stream_.close();
}

lsn_t WAL::NextLSN() {
    return ++current_lsn_;
}

lsn_t WAL::AppendBegin(txn_id_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    LogRecord rec{};
    rec.lsn      = NextLSN();
    rec.prev_lsn = INVALID_LSN;
    rec.txn_id   = txn_id;
    rec.type     = LogRecordType::BEGIN;
    log_stream_.write(reinterpret_cast<const char*>(&rec), sizeof(LogRecord));
    return rec.lsn;
}

lsn_t WAL::AppendCommit(txn_id_t txn_id, lsn_t prev_lsn) {
    std::lock_guard<std::mutex> lock(mutex_);
    LogRecord rec{};
    rec.lsn      = NextLSN();
    rec.prev_lsn = prev_lsn;
    rec.txn_id   = txn_id;
    rec.type     = LogRecordType::COMMIT;
    log_stream_.write(reinterpret_cast<const char*>(&rec), sizeof(LogRecord));
    log_stream_.flush();
    return rec.lsn;
}

lsn_t WAL::AppendAbort(txn_id_t txn_id, lsn_t prev_lsn) {
    std::lock_guard<std::mutex> lock(mutex_);
    LogRecord rec{};
    rec.lsn      = NextLSN();
    rec.prev_lsn = prev_lsn;
    rec.txn_id   = txn_id;
    rec.type     = LogRecordType::ABORT;
    log_stream_.write(reinterpret_cast<const char*>(&rec), sizeof(LogRecord));
    log_stream_.flush();
    return rec.lsn;
}

lsn_t WAL::AppendInsert(txn_id_t txn_id, lsn_t prev_lsn,
                         uint32_t page_id, uint16_t slot_id,
                         const char* data, uint32_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    LogRecord rec{};
    rec.lsn         = NextLSN();
    rec.prev_lsn    = prev_lsn;
    rec.txn_id      = txn_id;
    rec.type        = LogRecordType::INSERT;
    rec.page_id     = page_id;
    rec.slot_id     = slot_id;
    rec.data_length = length;
    log_stream_.write(reinterpret_cast<const char*>(&rec), sizeof(LogRecord));
    log_stream_.write(data, length);
    return rec.lsn;
}

lsn_t WAL::AppendDelete(txn_id_t txn_id, lsn_t prev_lsn,
                         uint32_t page_id, uint16_t slot_id,
                         const char* old_data, uint32_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    LogRecord rec{};
    rec.lsn         = NextLSN();
    rec.prev_lsn    = prev_lsn;
    rec.txn_id      = txn_id;
    rec.type        = LogRecordType::DELETE;
    rec.page_id     = page_id;
    rec.slot_id     = slot_id;
    rec.data_length = length;
    log_stream_.write(reinterpret_cast<const char*>(&rec), sizeof(LogRecord));
    log_stream_.write(old_data, length);
    return rec.lsn;
}

void WAL::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_stream_.is_open()) log_stream_.flush();
}

} // namespace FarhanDB
