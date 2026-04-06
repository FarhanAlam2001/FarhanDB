#include "storage/disk_manager.h"
#include <stdexcept>
#include <cstring>

namespace FarhanDB {

DiskManager::DiskManager(const std::string& db_file)
    : db_file_(db_file), page_count_(0) {

    // Open file for read/write, create if not exists
    file_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        file_.open(db_file_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file_.is_open()) {
        throw std::runtime_error("DiskManager: cannot open file " + db_file_);
    }

    // Calculate existing page count
    file_.seekg(0, std::ios::end);
    auto size = file_.tellg();
    page_count_ = static_cast<uint32_t>(size / PAGE_SIZE);
}

DiskManager::~DiskManager() {
    if (file_.is_open()) file_.close();
}

bool DiskManager::ReadPage(page_id_t page_id, Page& page) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (page_id >= page_count_) return false;

    file_.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE, std::ios::beg);
    file_.read(page.GetData(), PAGE_SIZE);
    return file_.good();
}

bool DiskManager::WritePage(page_id_t page_id, const Page& page) {
    std::lock_guard<std::mutex> lock(mutex_);

    file_.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE, std::ios::beg);
    file_.write(page.GetData(), PAGE_SIZE);
    file_.flush();
    return file_.good();
}

page_id_t DiskManager::AllocatePage() {
    std::lock_guard<std::mutex> lock(mutex_);
    page_id_t new_page = page_count_++;

    // Extend file
    file_.seekp(static_cast<std::streamoff>(new_page) * PAGE_SIZE, std::ios::beg);
    char empty[PAGE_SIZE] = {};
    file_.write(empty, PAGE_SIZE);
    file_.flush();

    return new_page;
}

void DiskManager::DeallocatePage(page_id_t page_id) {
    // Mark as free — simplified: just zero out
    std::lock_guard<std::mutex> lock(mutex_);
    file_.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE, std::ios::beg);
    char empty[PAGE_SIZE] = {};
    file_.write(empty, PAGE_SIZE);
    file_.flush();
}

uint32_t DiskManager::GetPageCount() const {
    return page_count_;
}

} // namespace FarhanDB
