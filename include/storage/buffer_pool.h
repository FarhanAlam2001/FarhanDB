#pragma once
#include "storage/page.h"
#include "storage/disk_manager.h"
#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace FarhanDB {

constexpr size_t BUFFER_POOL_SIZE = 256;

struct Frame {
    Page     page;
    int      pin_count  = 0;
    bool     is_dirty   = false;
};

class BufferPoolManager {
public:
    BufferPoolManager(size_t pool_size, DiskManager* disk_manager);
    ~BufferPoolManager();

    Page*   FetchPage(page_id_t page_id);
    bool    UnpinPage(page_id_t page_id, bool is_dirty);
    Page*   NewPage(page_id_t& out_page_id);
    bool    FlushPage(page_id_t page_id);
    void    FlushAllPages();
    bool    DeletePage(page_id_t page_id);
    size_t  GetPoolSize() const { return pool_size_; }

private:
    size_t                                          pool_size_;
    DiskManager*                                    disk_manager_;
    std::vector<Frame>                              frames_;
    std::unordered_map<page_id_t, size_t>           page_table_;
    std::list<size_t>                               lru_list_;
    std::unordered_map<size_t,
        std::list<size_t>::iterator>                lru_map_;
    std::mutex                                      mutex_;

    size_t  EvictFrame();
    void    UpdateLRU(size_t frame_idx);
};

} // namespace FarhanDB