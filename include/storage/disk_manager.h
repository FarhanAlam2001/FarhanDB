#pragma once
#include "storage/page.h"
#include <string>
#include <fstream>
#include <mutex>

namespace FarhanDB {

class DiskManager {
public:
    explicit DiskManager(const std::string& db_file);
    ~DiskManager();

    bool ReadPage(page_id_t page_id, Page& page);
    bool WritePage(page_id_t page_id, const Page& page);
    page_id_t AllocatePage();
    void DeallocatePage(page_id_t page_id);
    uint32_t GetPageCount() const;

private:
    std::string     db_file_;
    std::fstream    file_;
    uint32_t        page_count_;
    std::mutex      mutex_;
};

} // namespace FarhanDB
