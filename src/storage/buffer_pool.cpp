#include "storage/buffer_pool.h"
#include <stdexcept>

namespace FarhanDB {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
    frames_.resize(pool_size_);
    // All frames start in LRU list as free
    for (size_t i = 0; i < pool_size_; i++) {
        lru_list_.push_back(i);
        lru_map_[i] = std::prev(lru_list_.end());
    }
}

BufferPoolManager::~BufferPoolManager() {
    FlushAllPages();
}

void BufferPoolManager::UpdateLRU(size_t frame_idx) {
    // Move to front (most recently used)
    if (lru_map_.count(frame_idx)) {
        lru_list_.erase(lru_map_[frame_idx]);
    }
    lru_list_.push_front(frame_idx);
    lru_map_[frame_idx] = lru_list_.begin();
}

size_t BufferPoolManager::EvictFrame() {
    // Find unpinned frame from back of LRU
    for (auto it = lru_list_.rbegin(); it != lru_list_.rend(); ++it) {
        size_t idx = *it;
        if (frames_[idx].pin_count == 0) {
            if (frames_[idx].is_dirty) {
                disk_manager_->WritePage(frames_[idx].page.GetPageId(), frames_[idx].page);
            }
            page_id_t old_id = frames_[idx].page.GetPageId();
            if (old_id != INVALID_PAGE_ID) {
                page_table_.erase(old_id);
            }
            lru_list_.erase(lru_map_[idx]);
            lru_map_.erase(idx);
            frames_[idx].pin_count = 0;
            frames_[idx].is_dirty  = false;
            return idx;
        }
    }
    throw std::runtime_error("BufferPool: no evictable frame — all pages pinned!");
}

Page* BufferPoolManager::FetchPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Already in buffer pool?
    if (page_table_.count(page_id)) {
        size_t idx = page_table_[page_id];
        frames_[idx].pin_count++;
        UpdateLRU(idx);
        return &frames_[idx].page;
    }

    // Evict a frame and load from disk
    size_t idx = EvictFrame();
    disk_manager_->ReadPage(page_id, frames_[idx].page);
    frames_[idx].pin_count = 1;
    frames_[idx].is_dirty  = false;
    page_table_[page_id]   = idx;
    UpdateLRU(idx);
    return &frames_[idx].page;
}

bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!page_table_.count(page_id)) return false;

    size_t idx = page_table_[page_id];
    if (frames_[idx].pin_count <= 0) return false;
    frames_[idx].pin_count--;
    if (is_dirty) frames_[idx].is_dirty = true;
    return true;
}

Page* BufferPoolManager::NewPage(page_id_t& out_page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t idx = EvictFrame();

    out_page_id = disk_manager_->AllocatePage();
    frames_[idx].page.Reset(out_page_id);
    frames_[idx].pin_count = 1;
    frames_[idx].is_dirty  = true;
    page_table_[out_page_id] = idx;
    UpdateLRU(idx);
    return &frames_[idx].page;
}

bool BufferPoolManager::FlushPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!page_table_.count(page_id)) return false;

    size_t idx = page_table_[page_id];
    disk_manager_->WritePage(page_id, frames_[idx].page);
    frames_[idx].is_dirty = false;
    return true;
}

void BufferPoolManager::FlushAllPages() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [page_id, idx] : page_table_) {
        if (frames_[idx].is_dirty) {
            disk_manager_->WritePage(page_id, frames_[idx].page);
            frames_[idx].is_dirty = false;
        }
    }
}

bool BufferPoolManager::DeletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!page_table_.count(page_id)) return false;

    size_t idx = page_table_[page_id];
    if (frames_[idx].pin_count > 0) return false;

    page_table_.erase(page_id);
    disk_manager_->DeallocatePage(page_id);
    frames_[idx].page.Reset(INVALID_PAGE_ID);
    frames_[idx].is_dirty  = false;
    frames_[idx].pin_count = 0;
    return true;
}

} // namespace FarhanDB
