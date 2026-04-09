#include "storage/page.h"
#include <cstring>
#include <stdexcept>

namespace FarhanDB {

Page::Page() {
    std::memset(data_, 0, PAGE_SIZE);
    Header()->page_id           = INVALID_PAGE_ID;
    Header()->free_space_offset = sizeof(PageHeader);
    Header()->slot_count        = 0;
    Header()->free_space        = PAGE_SIZE - sizeof(PageHeader);
    Header()->is_dirty          = 0;
}

Page::Page(page_id_t id) : Page() {
    Header()->page_id = id;
}

page_id_t Page::GetPageId() const { return Header()->page_id; }
void      Page::SetPageId(page_id_t id) { Header()->page_id = id; }
bool      Page::IsDirty() const { return Header()->is_dirty != 0; }
void      Page::SetDirty(bool dirty) { Header()->is_dirty = dirty ? 1 : 0; }
uint16_t  Page::GetFreeSpace() const { return Header()->free_space; }

slot_id_t Page::InsertRecord(const char* data, uint16_t length) {
    if (Header()->free_space < length + sizeof(Slot)) return UINT16_MAX;

    // Place record at free_space_offset
    uint16_t offset = Header()->free_space_offset;
    std::memcpy(data_ + offset, data, length);

    // Add slot entry
    slot_id_t slot_id = Header()->slot_count;
    Slots()[slot_id].offset = offset;
    Slots()[slot_id].length = length;

    Header()->free_space_offset += length;
    Header()->slot_count++;
    Header()->free_space -= (length + sizeof(Slot));
    Header()->is_dirty = 1;

    return slot_id;
}

bool Page::GetRecord(slot_id_t slot, char* out_data, uint16_t& out_length) const {
    if (slot >= Header()->slot_count) return false;
    if (Slots()[slot].length == 0) return false;

    out_length = Slots()[slot].length;
    std::memcpy(out_data, data_ + Slots()[slot].offset, out_length);
    return true;
}

bool Page::DeleteRecord(slot_id_t slot) {
    if (slot >= Header()->slot_count) return false;
    Slots()[slot].length = 0;
    Header()->is_dirty = 1;
    return true;
}

char*       Page::GetData()       { return data_; }
const char* Page::GetData() const { return data_; }

void Page::Reset(page_id_t id) {
    std::memset(data_, 0, PAGE_SIZE);
    Header()->page_id           = id;
    Header()->free_space_offset = sizeof(PageHeader);
    Header()->slot_count        = 0;
    Header()->free_space        = PAGE_SIZE - sizeof(PageHeader);
    Header()->is_dirty          = 0;
}

PageHeader*       Page::Header()       { return reinterpret_cast<PageHeader*>(data_); }
const PageHeader* Page::Header() const { return reinterpret_cast<const PageHeader*>(data_); }

Slot*       Page::Slots()       {
    return reinterpret_cast<Slot*>(data_ + sizeof(PageHeader));
}
const Slot* Page::Slots() const {
    return reinterpret_cast<const Slot*>(data_ + sizeof(PageHeader));
}

} // namespace FarhanDB
