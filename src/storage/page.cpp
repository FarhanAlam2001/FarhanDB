#include "storage/page.h"
#include <cstring>
#include <stdexcept>

namespace FarhanDB {

Page::Page() {
    std::memset(data_, 0, PAGE_SIZE);
    Header()->page_id           = INVALID_PAGE_ID;
    Header()->free_space_offset = PAGE_SIZE;  // records grow backwards from end
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
    // Space needed: length for record + sizeof(Slot) for slot entry
    uint16_t space_needed = length + sizeof(Slot);
    if (Header()->free_space < space_needed) return UINT16_MAX;

    // Slots grow forward from after PageHeader
    // Records grow backward from end of page
    uint16_t slot_end    = sizeof(PageHeader) + (Header()->slot_count + 1) * sizeof(Slot);
    uint16_t record_start = Header()->free_space_offset - length;

    // Make sure they don't overlap
    if (record_start < slot_end) return UINT16_MAX;

    // Write record at record_start (growing from end backwards)
    Header()->free_space_offset = record_start;
    std::memcpy(data_ + record_start, data, length);

    // Add slot entry (growing forward from PageHeader)
    slot_id_t slot_id = Header()->slot_count;
    Slots()[slot_id].offset = record_start;
    Slots()[slot_id].length = length;

    Header()->slot_count++;
    Header()->free_space -= space_needed;
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
    Header()->free_space_offset = PAGE_SIZE;  // records grow backwards from end
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
