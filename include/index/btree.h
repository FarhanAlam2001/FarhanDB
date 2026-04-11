#pragma once
#include "storage/buffer_pool.h"
#include <string>
#include <vector>
#include <optional>

namespace FarhanDB {

constexpr size_t BTREE_ORDER = 100; // max keys per node

using KeyType = int32_t;
using RID = std::pair<page_id_t, slot_id_t>; // Record ID

struct BTreeNode {
    bool        is_leaf;
    uint32_t    key_count;
    KeyType     keys[BTREE_ORDER];
    // For internal nodes: children[BTREE_ORDER + 1]
    // For leaf nodes:     values[BTREE_ORDER], next_leaf
    page_id_t   children[BTREE_ORDER + 1];
    RID         values[BTREE_ORDER];
    page_id_t   next_leaf; // linked list of leaves
};

class BTree {
public:
    // existing_root = INVALID_PAGE_ID → create fresh tree
    // existing_root = valid id        → load tree from disk
    BTree(const std::string& name, BufferPoolManager* bpm,
          page_id_t existing_root = INVALID_PAGE_ID);

    bool                        Insert(KeyType key, RID rid);
    bool                        Delete(KeyType key);
    std::optional<RID>          Search(KeyType key);
    std::vector<RID>            RangeSearch(KeyType low, KeyType high);

    page_id_t   GetRootPageId() const { return root_page_id_; }

private:
    std::string         name_;
    BufferPoolManager*  bpm_;
    page_id_t           root_page_id_;

    BTreeNode*  FetchNode(page_id_t page_id);
    void        UnpinNode(page_id_t page_id, bool dirty);
    page_id_t   AllocateNode();

    void        InsertInternal(page_id_t node_id, KeyType key, RID rid,
                               KeyType& pushed_key, page_id_t& new_child);
    bool        DeleteInternal(page_id_t node_id, KeyType key);
    void        SplitLeaf(page_id_t leaf_id, KeyType& pushed_key, page_id_t& new_leaf);
    void        SplitInternal(page_id_t node_id, KeyType& pushed_key, page_id_t& new_node);
};

} // namespace FarhanDB
