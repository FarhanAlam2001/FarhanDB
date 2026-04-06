#pragma once
#include <cstdint>
#include <cstring>
#include <array>

namespace FarhanDB {

// Page size: 4KB
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t MAX_PAGES = 10000;

using page_id_t = uint32_t;
using slot_id_t = uint16_t;

constexpr page_id_t INVALID_PAGE_ID = UINT32_MAX;

// Slotted page header
struct PageHeader {
    page_id_t   page_id;
    uint16_t    free_space_offset;  // where free space starts
    uint16_t    slot_count;
    uint16_t    free_space;
    uint8_t     is_dirty;
    uint8_t     reserved;
};

// Slot directory entry
struct Slot {
    uint16_t offset;  // offset of record in page
    uint16_t length;  // length of record
};

class Page {
public:
    Page();
    explicit Page(page_id_t id);

    page_id_t   GetPageId() const;
    void        SetPageId(page_id_t id);
    bool        IsDirty() const;
    void        SetDirty(bool dirty);
    uint16_t    GetFreeSpace() const;

    // Record operations
    slot_id_t   InsertRecord(const char* data, uint16_t length);
    bool        GetRecord(slot_id_t slot, char* out_data, uint16_t& out_length) const;
    bool        DeleteRecord(slot_id_t slot);

    // Raw page data access (for disk I/O)
    char*       GetData();
    const char* GetData() const;

    void        Reset(page_id_t id);

private:
    alignas(PAGE_SIZE) char data_[PAGE_SIZE];

    PageHeader* Header();
    const PageHeader* Header() const;
    Slot*       Slots();
    const Slot* Slots() const;
};

} // namespace FarhanDB
