#include "index/btree.h"
#include "storage/page.h"
#include <cstring>
#include <stdexcept>

namespace FarhanDB {

// Place BTreeNode after PageHeader so SetPageId() never corrupts node data
static constexpr size_t NODE_OFFSET = sizeof(PageHeader);

// ─── Constructor ─────────────────────────────────────────────────────────────
BTree::BTree(const std::string& name, BufferPoolManager* bpm, page_id_t existing_root)
    : name_(name), bpm_(bpm), root_page_id_(existing_root) {

    if (existing_root == INVALID_PAGE_ID) {
        // Fresh tree — allocate a root leaf node
        page_id_t pid;
        Page* page = bpm_->NewPage(pid);
        if (!page) throw std::runtime_error("BTree: cannot allocate root page");

        BTreeNode* root = reinterpret_cast<BTreeNode*>(page->GetData() + NODE_OFFSET);
        *root = BTreeNode{};
        root->is_leaf   = true;
        root->key_count = 0;
        root->next_leaf = INVALID_PAGE_ID;

        root_page_id_ = pid;
        bpm_->UnpinPage(pid, true);
    }
    // else: loading existing tree from disk — root_page_id_ already set
}

// ─── Internal helpers ─────────────────────────────────────────────────────────
BTreeNode* BTree::FetchNode(page_id_t page_id) {
    Page* page = bpm_->FetchPage(page_id);
    if (!page) return nullptr;
    // Skip PageHeader — BTreeNode starts at NODE_OFFSET
    return reinterpret_cast<BTreeNode*>(page->GetData() + NODE_OFFSET);
}

void BTree::UnpinNode(page_id_t page_id, bool dirty) {
    bpm_->UnpinPage(page_id, dirty);
}

page_id_t BTree::AllocateNode() {
    page_id_t pid;
    Page* page = bpm_->NewPage(pid);
    if (!page) return INVALID_PAGE_ID;

    BTreeNode* node = reinterpret_cast<BTreeNode*>(page->GetData() + NODE_OFFSET);
    *node = BTreeNode{};
    node->next_leaf = INVALID_PAGE_ID;

    bpm_->UnpinPage(pid, true);
    return pid;
}

// ─── Search ──────────────────────────────────────────────────────────────────
std::optional<RID> BTree::Search(KeyType key) {
    if (root_page_id_ == INVALID_PAGE_ID) return std::nullopt;

    page_id_t cur_id = root_page_id_;

    while (true) {
        BTreeNode* node = FetchNode(cur_id);
        if (!node) return std::nullopt;

        if (node->is_leaf) {
            // Linear scan within leaf
            for (uint32_t i = 0; i < node->key_count; i++) {
                if (node->keys[i] == key) {
                    RID result = node->values[i];
                    UnpinNode(cur_id, false);
                    return result;
                }
            }
            UnpinNode(cur_id, false);
            return std::nullopt;
        }

        // Internal node: find correct child
        uint32_t i = 0;
        while (i < node->key_count && key >= node->keys[i]) i++;
        page_id_t child = node->children[i];
        UnpinNode(cur_id, false);
        cur_id = child;
    }
}

// ─── Range Search ─────────────────────────────────────────────────────────────
std::vector<RID> BTree::RangeSearch(KeyType low, KeyType high) {
    std::vector<RID> results;
    if (root_page_id_ == INVALID_PAGE_ID) return results;

    // Step 1: Descend to the leaf that contains 'low'
    page_id_t cur_id = root_page_id_;
    while (true) {
        BTreeNode* node = FetchNode(cur_id);
        if (!node) return results;
        if (node->is_leaf) { UnpinNode(cur_id, false); break; }

        uint32_t i = 0;
        while (i < node->key_count && low >= node->keys[i]) i++;
        page_id_t child = node->children[i];
        UnpinNode(cur_id, false);
        cur_id = child;
    }

    // Step 2: Walk the leaf linked list collecting keys in [low, high]
    while (cur_id != INVALID_PAGE_ID) {
        BTreeNode* node = FetchNode(cur_id);
        if (!node) break;

        bool done = false;
        for (uint32_t i = 0; i < node->key_count; i++) {
            if (node->keys[i] > high) { done = true; break; }
            if (node->keys[i] >= low) results.push_back(node->values[i]);
        }

        page_id_t next = node->next_leaf;
        UnpinNode(cur_id, false);
        if (done) break;
        cur_id = next;
    }

    return results;
}

// ─── Insert ──────────────────────────────────────────────────────────────────
bool BTree::Insert(KeyType key, RID rid) {
    if (root_page_id_ == INVALID_PAGE_ID) return false;

    KeyType   pushed_key = 0;
    page_id_t new_child  = INVALID_PAGE_ID;

    InsertInternal(root_page_id_, key, rid, pushed_key, new_child);

    // Root was split — promote to a new root
    if (new_child != INVALID_PAGE_ID) {
        page_id_t new_root_id = AllocateNode();
        BTreeNode* new_root = FetchNode(new_root_id);
        if (!new_root) return false;

        new_root->is_leaf     = false;
        new_root->key_count   = 1;
        new_root->keys[0]     = pushed_key;
        new_root->children[0] = root_page_id_;
        new_root->children[1] = new_child;

        UnpinNode(new_root_id, true);
        root_page_id_ = new_root_id;
    }

    return true;
}

void BTree::InsertInternal(page_id_t node_id, KeyType key, RID rid,
                            KeyType& pushed_key, page_id_t& new_child) {
    BTreeNode* node = FetchNode(node_id);
    if (!node) return;

    if (node->is_leaf) {
        // Shift keys right to find sorted insertion position
        uint32_t i = node->key_count;
        while (i > 0 && node->keys[i - 1] > key) {
            node->keys[i]   = node->keys[i - 1];
            node->values[i] = node->values[i - 1];
            i--;
        }
        node->keys[i]   = key;
        node->values[i] = rid;
        node->key_count++;

        if (node->key_count < BTREE_ORDER) {
            // No split needed
            UnpinNode(node_id, true);
            new_child = INVALID_PAGE_ID;
        } else {
            // Leaf is full — split it
            UnpinNode(node_id, true);
            SplitLeaf(node_id, pushed_key, new_child);
        }

    } else {
        // Internal node: find which child to descend into
        uint32_t i = 0;
        while (i < node->key_count && key >= node->keys[i]) i++;
        page_id_t child_id = node->children[i];
        UnpinNode(node_id, false); // unpin before recursion

        KeyType   child_pushed = 0;
        page_id_t child_new    = INVALID_PAGE_ID;
        InsertInternal(child_id, key, rid, child_pushed, child_new);

        if (child_new == INVALID_PAGE_ID) {
            new_child = INVALID_PAGE_ID;
            return; // child did not split
        }

        // Child split — re-fetch this node and insert the promoted key
        node = FetchNode(node_id);
        if (!node) return;

        uint32_t j = node->key_count;
        while (j > i) {
            node->keys[j]         = node->keys[j - 1];
            node->children[j + 1] = node->children[j];
            j--;
        }
        node->keys[i]         = child_pushed;
        node->children[i + 1] = child_new;
        node->key_count++;

        if (node->key_count < BTREE_ORDER) {
            UnpinNode(node_id, true);
            new_child = INVALID_PAGE_ID;
        } else {
            UnpinNode(node_id, true);
            SplitInternal(node_id, pushed_key, new_child);
        }
    }
}

// ─── Split helpers ────────────────────────────────────────────────────────────
void BTree::SplitLeaf(page_id_t leaf_id, KeyType& pushed_key, page_id_t& new_leaf) {
    BTreeNode* leaf = FetchNode(leaf_id);
    if (!leaf) { new_leaf = INVALID_PAGE_ID; return; }

    new_leaf = AllocateNode();
    BTreeNode* new_node = FetchNode(new_leaf);
    if (!new_node) { UnpinNode(leaf_id, false); new_leaf = INVALID_PAGE_ID; return; }

    uint32_t split = leaf->key_count / 2;

    new_node->is_leaf   = true;
    new_node->key_count = leaf->key_count - split;
    new_node->next_leaf = leaf->next_leaf; // link new leaf into chain
    leaf->next_leaf     = new_leaf;
    leaf->key_count     = split;

    for (uint32_t i = 0; i < new_node->key_count; i++) {
        new_node->keys[i]   = leaf->keys[split + i];
        new_node->values[i] = leaf->values[split + i];
    }

    pushed_key = new_node->keys[0]; // smallest key of new leaf goes up

    UnpinNode(new_leaf, true);
    UnpinNode(leaf_id, true);
}

void BTree::SplitInternal(page_id_t node_id, KeyType& pushed_key, page_id_t& new_node_id) {
    BTreeNode* node = FetchNode(node_id);
    if (!node) { new_node_id = INVALID_PAGE_ID; return; }

    new_node_id = AllocateNode();
    BTreeNode* new_node = FetchNode(new_node_id);
    if (!new_node) { UnpinNode(node_id, false); new_node_id = INVALID_PAGE_ID; return; }

    uint32_t split = node->key_count / 2;
    pushed_key = node->keys[split]; // middle key gets pushed up

    new_node->is_leaf   = false;
    new_node->key_count = node->key_count - split - 1;

    for (uint32_t i = 0; i < new_node->key_count; i++) {
        new_node->keys[i]     = node->keys[split + 1 + i];
        new_node->children[i] = node->children[split + 1 + i];
    }
    new_node->children[new_node->key_count] = node->children[node->key_count];

    node->key_count = split;

    UnpinNode(new_node_id, true);
    UnpinNode(node_id, true);
}

// ─── Delete ──────────────────────────────────────────────────────────────────
bool BTree::Delete(KeyType key) {
    if (root_page_id_ == INVALID_PAGE_ID) return false;
    return DeleteInternal(root_page_id_, key);
}

bool BTree::DeleteInternal(page_id_t node_id, KeyType key) {
    BTreeNode* node = FetchNode(node_id);
    if (!node) return false;

    if (node->is_leaf) {
        // Find and remove the key
        for (uint32_t i = 0; i < node->key_count; i++) {
            if (node->keys[i] == key) {
                for (uint32_t j = i; j < node->key_count - 1; j++) {
                    node->keys[j]   = node->keys[j + 1];
                    node->values[j] = node->values[j + 1];
                }
                node->key_count--;
                UnpinNode(node_id, true);
                return true;
            }
        }
        UnpinNode(node_id, false);
        return false;
    }

    // Internal node: route to correct child
    uint32_t i = 0;
    while (i < node->key_count && key >= node->keys[i]) i++;
    page_id_t child = node->children[i];
    UnpinNode(node_id, false);
    return DeleteInternal(child, key);
}

} // namespace FarhanDB
