#include "bplus_tree.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

int BPlusTree::find_child_index(std::shared_ptr<BPlusNode> node, int key) {
    // Equivalent to the old linear scan (first idx with key < keys[idx]),
    // but O(log fanout) instead of O(fanout).
    auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
    return static_cast<int>(std::distance(node->keys.begin(), it));
}

int BPlusTree::find_child_index_range(std::shared_ptr<BPlusNode> node, int key) {
    // Range scans can start on the first child whose subtree may contain
    // `key` or anything larger, so we use lower_bound here to bias left.
    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    return static_cast<int>(std::distance(node->keys.begin(), it));
}

// OPTIMIZATION #7: Binary search to find key in leaf
int BPlusTree::find_key_index(std::shared_ptr<BPlusNode> leaf, int key) {
    // Returns index if found, -1 otherwise
    if (!leaf->is_leaf || leaf->keys.empty()) return -1;
    
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it != leaf->keys.end() && *it == key) {
        return std::distance(leaf->keys.begin(), it);
    }
    return -1;
}

// OPTIMIZATION #8: Binary search for insert position in leaf
int BPlusTree::find_insert_position(std::shared_ptr<BPlusNode> leaf, int key) {
    // Returns position where key should be inserted
    if (!leaf->is_leaf) return -1;
    
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    return std::distance(leaf->keys.begin(), it);
}

void BPlusTree::insert(int key, const std::string& value) {
    if (root->is_full()) {
        split_root(root);
    }
    
    auto node = root;
    std::vector<std::shared_ptr<BPlusNode>> path;
    std::vector<int> child_indices;
    path.push_back(node);

    while (!node->is_leaf) {
        int idx = find_child_index(node, key);
        auto child = node->children[idx];

        if (child->is_full()) {
            split_child(node, idx);
            if (key >= node->keys[idx]) {
                idx++;
            }
            child = node->children[idx];
        }

        child_indices.push_back(idx);
        node = child;
        path.push_back(node);
    }

    int idx = find_insert_position(node, key);
    if (idx < node->keys.size() && node->keys[idx] == key) {
        node->values[idx] = value;
        return;
    }

    node->keys.insert(node->keys.begin() + idx, key);
    node->values.insert(node->values.begin() + idx, value);

    if (node->is_full()) {
        if (node == root) {
            split_root(node);
        } else {
            split_child(path[path.size() - 2], child_indices.back());
        }
    }
}

// ============================================================================
// Split helpers with explicit parent/index context
// ============================================================================
void BPlusTree::split_root(std::shared_ptr<BPlusNode> full_root) {
    auto new_root = std::make_shared<BPlusNode>(false);
    new_root->children.push_back(full_root);
    split_child(new_root, 0);
    root = new_root;
}

// ============================================================================
// Split helpers with explicit sizing
// ============================================================================

void BPlusTree::split_child(std::shared_ptr<BPlusNode> parent, int child_index) {
    auto child = parent->children[child_index];

    if (child->is_leaf) {
        auto new_leaf = std::make_shared<BPlusNode>(true);

        int total_keys = static_cast<int>(child->keys.size());
        int left_size = total_keys / 2;

        new_leaf->keys.assign(child->keys.begin() + left_size, child->keys.end());
        new_leaf->values.assign(child->values.begin() + left_size, child->values.end());

        child->keys.erase(child->keys.begin() + left_size, child->keys.end());
        child->values.erase(child->values.begin() + left_size, child->values.end());

        new_leaf->next = child->next;
        child->next = new_leaf;

        int promoted_key = new_leaf->keys.front();
        parent->keys.insert(parent->keys.begin() + child_index, promoted_key);
        parent->children.insert(parent->children.begin() + child_index + 1, new_leaf);
        return;
    }

    auto new_internal = std::make_shared<BPlusNode>(false);

    int total_keys = static_cast<int>(child->keys.size());
    int left_size = total_keys / 2;
    int promoted_key = child->keys[left_size];
    int child_split = left_size + 1;

    new_internal->keys.assign(child->keys.begin() + left_size + 1, child->keys.end());
    child->keys.erase(child->keys.begin() + left_size, child->keys.end());

    new_internal->children.assign(child->children.begin() + child_split, child->children.end());
    child->children.erase(child->children.begin() + child_split, child->children.end());

    parent->keys.insert(parent->keys.begin() + child_index, promoted_key);
    parent->children.insert(parent->children.begin() + child_index + 1, new_internal);
}

// ============================================================================
// Rest of operations remain similar, just use node->is_full() and node->is_underfull()
// ============================================================================

bool BPlusTree::search(int key, std::string& value) {
    auto node = root;
    
    // Traverse to leaf
    while (!node->is_leaf) {
        int idx = find_child_index(node, key);
        node = node->children[idx];
    }
    
    // OPTIMIZATION #7: Use binary search in leaf instead of linear scan
    int idx = find_key_index(node, key);
    if (idx != -1) {
        value = node->values[idx];
        return true;
    }
    
    return false;
}

std::vector<Entry> BPlusTree::range_scan(int min_key, int max_key) {
    std::vector<Entry> result;
    
    auto node = root;
    while (!node->is_leaf) {
        int idx = find_child_index_range(node, min_key);
        node = node->children[idx];
    }
    
    while (node != nullptr) {
        for (int i = 0; i < node->keys.size(); i++) {
            if (node->keys[i] > max_key) {
                return result;
            }
            if (node->keys[i] >= min_key) {
                result.push_back({node->keys[i], node->values[i]});
            }
        }
        node = node->next;
    }
    
    return result;
}

// Walks to the leftmost leaf of `node`'s subtree and returns its first key.
// An internal node's own keys[0] is a routing/separator value, NOT
// necessarily the subtree's true minimum, so this must descend rather than
// just read node->keys.front().
int BPlusTree::subtree_min(std::shared_ptr<BPlusNode> node) const {
    auto cur = node;
    while (cur && !cur->is_leaf) {
        if (cur->children.empty()) return INT_MAX;
        cur = cur->children.front();
    }
    if (!cur || cur->keys.empty()) return INT_MAX;
    return cur->keys.front();
}

bool BPlusTree::remove(int key) {
    std::vector<std::shared_ptr<BPlusNode>> path;
    std::vector<int> child_indices;
    auto node = root;
    path.push_back(node);

    while (!node->is_leaf) {
        int idx = find_child_index(node, key);
        child_indices.push_back(idx);
        node = node->children[idx];
        path.push_back(node);
    }
    
    // OPTIMIZATION #7: Use binary search to find key in leaf
    int idx = find_key_index(node, key);
    
    if (idx == -1) {
        return false;
    }
    
    node->keys.erase(node->keys.begin() + idx);
    node->values.erase(node->values.begin() + idx);
    
    // Deleting the first key changes the subtree minimum, so update the
    // first ancestor separator that actually refers to this leaf path.
    if (idx == 0 && !node->keys.empty()) {
        update_separator_upwards(path, child_indices, path.size() - 1, node->keys[0]);
    }
    
    // Note: `node` is always a leaf here (we just deleted from one), so a
    // root-collapses-to-one-child scenario can never be detected by
    // inspecting `node` itself - it's `node`'s parent (an internal node)
    // that can collapse to a single child. That case is handled inside
    // merge_or_redistribute_leaf() below.
    if (node != root && node->is_underfull()) {
        merge_or_redistribute_leaf(path, child_indices);
    }
    
    return true;
}

void BPlusTree::update_separator_upwards(
    const std::vector<std::shared_ptr<BPlusNode>>& path,
    const std::vector<int>& child_indices,
    size_t node_depth,
    int new_min) {
    if (node_depth == 0) {
        return;
    }

    int current_min = new_min;
    for (size_t depth = node_depth; depth > 0; --depth) {
        int idx = child_indices[depth - 1];
        if (idx > 0) {
            path[depth - 1]->keys[idx - 1] = current_min;
            return;
        }

        current_min = subtree_min(path[depth - 1]);
    }
}

void BPlusTree::merge_or_redistribute_leaf(
    const std::vector<std::shared_ptr<BPlusNode>>& path,
    const std::vector<int>& child_indices) {
    size_t leaf_depth = path.size() - 1;
    if (leaf_depth == 0) return;

    auto node = path[leaf_depth];
    auto parent = path[leaf_depth - 1];
    int idx = child_indices[leaf_depth - 1];
    
    // Try to borrow from left sibling
    if (idx > 0) {
        auto left_sibling = parent->children[idx - 1];
        if (left_sibling->keys.size() > (ORDER_LEAF - 1) / 2) {
            // Borrow from left
            int key = left_sibling->keys.back();
            std::string value = left_sibling->values.back();
            left_sibling->keys.pop_back();
            left_sibling->values.pop_back();
            
            node->keys.insert(node->keys.begin(), key);
            node->values.insert(node->values.begin(), value);
            
            parent->keys[idx - 1] = node->keys[0];
            return;
        }
    }
    
    // Try to borrow from right sibling
    if (idx < parent->children.size() - 1) {
        auto right_sibling = parent->children[idx + 1];
        if (right_sibling->keys.size() > (ORDER_LEAF - 1) / 2) {
            // Borrow from right
            int key = right_sibling->keys.front();
            std::string value = right_sibling->values.front();
            right_sibling->keys.erase(right_sibling->keys.begin());
            right_sibling->values.erase(right_sibling->values.begin());
            
            node->keys.push_back(key);
            node->values.push_back(value);
            
            parent->keys[idx] = right_sibling->keys[0];
            return;
        }
    }
    
    // Merge with right sibling
    if (idx < parent->children.size() - 1) {
        auto right_sibling = parent->children[idx + 1];
        node->keys.insert(node->keys.end(), right_sibling->keys.begin(), right_sibling->keys.end());
        node->values.insert(node->values.end(), right_sibling->values.begin(), right_sibling->values.end());
        node->next = right_sibling->next;
        
        // BUG FIX #3: Merge bug - must erase separator key when erasing child
        // After merge: node has all keys, right_sibling is gone
        // So parent must have: children-1 and keys-1
        parent->children.erase(parent->children.begin() + idx + 1);
        parent->keys.erase(parent->keys.begin() + idx);
    }
    // Merge with left sibling
    else {
        auto left_sibling = parent->children[idx - 1];
        left_sibling->keys.insert(left_sibling->keys.end(), node->keys.begin(), node->keys.end());
        left_sibling->values.insert(left_sibling->values.end(), node->values.begin(), node->values.end());
        left_sibling->next = node->next;
        
        parent->keys.erase(parent->keys.begin() + idx - 1);
        parent->children.erase(parent->children.begin() + idx);
    }
    
    // Recursively check parent
    if (idx == 0 && !node->keys.empty()) {
        update_separator_upwards(path, child_indices, leaf_depth, node->keys[0]);
    }

    if (parent != root && parent->is_underfull()) {
        merge_or_redistribute_internal(path, child_indices, leaf_depth - 1);
    } else if (parent == root && parent->keys.empty() && parent->children.size() == 1) {
        // CRITICAL FIX: a leaf-level merge can collapse the root (when it
        // had exactly 2 children) down to a single child. Without this,
        // root is left as an internal node with one child and zero keys -
        // the next underflow on that lone child has no sibling at all,
        // and merge_or_redistribute_*'s "merge with left sibling" branch
        // indexes parent->children[idx - 1] with idx == 0, reading out of
        // bounds. Promote the child to be the new root, exactly as
        // merge_or_redistribute_internal() already does for the
        // equivalent internal-level case below.
        root = parent->children[0];
    }
}

void BPlusTree::merge_or_redistribute_internal(
    const std::vector<std::shared_ptr<BPlusNode>>& path,
    const std::vector<int>& child_indices,
    size_t node_depth) {
    if (node_depth == 0) return;  // Root can have fewer keys

    auto node = path[node_depth];
    auto parent = path[node_depth - 1];
    int idx = child_indices[node_depth - 1];
    
    // Try to borrow from left sibling
    if (idx > 0) {
        auto left_sibling = parent->children[idx - 1];
        if (left_sibling->keys.size() > (ORDER_INTERNAL - 1) / 2) {
            // Borrow from left
            int key = left_sibling->keys.back();
            auto child = left_sibling->children.back();
            left_sibling->keys.pop_back();
            left_sibling->children.pop_back();
            
            node->keys.insert(node->keys.begin(), parent->keys[idx - 1]);
            node->children.insert(node->children.begin(), child);
            
            parent->keys[idx - 1] = key;
            return;
        }
    }
    
    // Try to borrow from right sibling
    if (idx < parent->children.size() - 1) {
        auto right_sibling = parent->children[idx + 1];
        if (right_sibling->keys.size() > (ORDER_INTERNAL - 1) / 2) {
            // Borrow from right
            int key = right_sibling->keys.front();
            auto child = right_sibling->children.front();
            right_sibling->keys.erase(right_sibling->keys.begin());
            right_sibling->children.erase(right_sibling->children.begin());
            
            node->keys.push_back(parent->keys[idx]);
            node->children.push_back(child);
            
            parent->keys[idx] = key;
            return;
        }
    }
    
    // Merge with right sibling
    if (idx < parent->children.size() - 1) {
        auto right_sibling = parent->children[idx + 1];
        node->keys.push_back(parent->keys[idx]);
        node->keys.insert(node->keys.end(), right_sibling->keys.begin(), right_sibling->keys.end());
        node->children.insert(node->children.end(), right_sibling->children.begin(), right_sibling->children.end());

        // BUG FIX #4: Internal merge bug - erase separator key when merging
        parent->children.erase(parent->children.begin() + idx + 1);
        parent->keys.erase(parent->keys.begin() + idx);
    }
    // Merge with left sibling
    else {
        auto left_sibling = parent->children[idx - 1];
        left_sibling->keys.push_back(parent->keys[idx - 1]);
        left_sibling->keys.insert(left_sibling->keys.end(), node->keys.begin(), node->keys.end());
        left_sibling->children.insert(left_sibling->children.end(), node->children.begin(), node->children.end());

        parent->keys.erase(parent->keys.begin() + idx - 1);
        parent->children.erase(parent->children.begin() + idx);
    }
    
    // Recursively check parent
    if (idx == 0 && !node->keys.empty()) {
        update_separator_upwards(path, child_indices, node_depth, subtree_min(node));
    }

    if (parent != root && parent->is_underfull()) {
        merge_or_redistribute_internal(path, child_indices, node_depth - 1);
    } else if (parent == root && parent->keys.size() == 0 && parent->children.size() == 1) {
        // Root became empty, promote its only child
        root = parent->children[0];
    }
}

void BPlusTree::print_tree() const {
    std::cout << "\n=== B+ Tree Structure ===\n";
    
    std::vector<std::vector<std::shared_ptr<BPlusNode>>> levels;
    std::vector<std::shared_ptr<BPlusNode>> current_level = {root};
    
    while (!current_level.empty()) {
        levels.push_back(current_level);
        std::vector<std::shared_ptr<BPlusNode>> next_level;
        
        for (auto& node : current_level) {
            if (!node->is_leaf) {
                for (auto& child : node->children) {
                    next_level.push_back(child);
                }
            }
        }
        
        current_level = next_level;
    }
    
    for (int level = 0; level < levels.size(); level++) {
        std::cout << "Level " << level << " (";
        if (level == 0) {
            std::cout << "root, fanout up to " << ORDER_INTERNAL;
        } else if (level == levels.size() - 1) {
            std::cout << "leaves, fanout up to " << ORDER_LEAF;
        } else {
            std::cout << "internal, fanout up to " << ORDER_INTERNAL;
        }
        std::cout << "): ";
        
        for (auto& node : levels[level]) {
            std::cout << "[";
            for (int i = 0; i < node->keys.size(); i++) {
                std::cout << node->keys[i];
                if (i < node->keys.size() - 1) std::cout << ", ";
            }
            std::cout << "] ";
        }
        std::cout << "\n";
    }
}

void BPlusTree::print_leaf_chain() const {
    std::cout << "\n=== Leaf Chain ===\n";
    auto leaf = leftmost_leaf;
    int count = 0;
    
    while (leaf != nullptr && count < 100) {
        std::cout << "[";
        for (int i = 0; i < leaf->keys.size(); i++) {
            std::cout << leaf->keys[i];
            if (i < leaf->keys.size() - 1) std::cout << ", ";
        }
        std::cout << "] → ";
        leaf = leaf->next;
        count++;
    }
    std::cout << "END\n";
}

void BPlusTree::print_stats() const {
    std::cout << "\n=== B+ Tree Configuration ===\n";
    std::cout << "Internal node fanout (ORDER_INTERNAL): " << ORDER_INTERNAL << "\n";
    std::cout << "  Max keys: " << (ORDER_INTERNAL - 1) << "\n";
    std::cout << "  Max children: " << ORDER_INTERNAL << "\n";
    std::cout << "\nLeaf node fanout (ORDER_LEAF): " << ORDER_LEAF << "\n";
    std::cout << "  Max keys: " << (ORDER_LEAF - 1) << "\n";
    std::cout << "  Max data entries: " << (ORDER_LEAF - 1) << "\n";
    std::cout << "\nFanout ratio (Internal/Leaf): " 
              << (double)ORDER_INTERNAL / ORDER_LEAF << "x\n";
}

// OPTIMIZATION #10: Comprehensive tree validation
bool BPlusTree::validate_node(std::shared_ptr<BPlusNode> node, int& leaf_depth, int current_depth, int& dfs_leaf_count) const {
    if (!node) return false;
    
    // Check 1: Keys are sorted
    for (int i = 0; i < (int)node->keys.size() - 1; i++) {
        if (node->keys[i] >= node->keys[i + 1]) {
            std::cerr << "ERROR: Keys not sorted at node\n";
            return false;
        }
    }
    
    // Check 2: No overflow
    if (node->is_leaf && node->keys.size() > ORDER_LEAF - 1) {
        std::cerr << "ERROR: Leaf node overflow (" << node->keys.size() << " > " << ORDER_LEAF - 1 << ")\n";
        return false;
    }
    if (!node->is_leaf && node->keys.size() > ORDER_INTERNAL - 1) {
        std::cerr << "ERROR: Internal node overflow\n";
        return false;
    }
    
    // Check 3: No underflow (except root)
    if (node != root) {
        if (node->is_leaf && node->keys.size() < (ORDER_LEAF - 1) / 2) {
            std::cerr << "ERROR: Leaf node underflow\n";
            return false;
        }
        if (!node->is_leaf && node->keys.size() < (ORDER_INTERNAL - 1) / 2) {
            std::cerr << "ERROR: Internal node underflow\n";
            return false;
        }
    }
    
    if (node->is_leaf) {
        // BUG FIX #4: Count leaves actually reached via DFS so validate()
        // can cross-check this against the leaf-chain traversal count -
        // a broken leaf->next pointer wouldn't otherwise be caught.
        dfs_leaf_count++;
        
        // Check 4: Leaf depth consistency
        if (leaf_depth == -1) {
            leaf_depth = current_depth;
        } else if (leaf_depth != current_depth) {
            std::cerr << "ERROR: Leaves at different depths\n";
            return false;
        }
        
        // Check 5: Values match keys
        if (node->keys.size() != node->values.size()) {
            std::cerr << "ERROR: Leaf keys/values mismatch\n";
            return false;
        }
    } else {
        // Check 6: Internal node has keys+1 children
        if (node->children.size() != node->keys.size() + 1) {
            std::cerr << "ERROR: Internal node children != keys+1\n";
            return false;
        }
        
        // Recursively validate children
        for (int i = 0; i < (int)node->children.size(); i++) {
            auto child = node->children[i];

            // Check 8: Separator keys correctness
            // BUG FIX #5 (corrected): keys[i-1] must equal the TRUE minimum
            // key of children[i]'s subtree. For a leaf child that's just
            // keys.front(), but for an internal child, child->keys.front()
            // is itself only a routing value - not the subtree minimum - so
            // we must descend to the leftmost leaf via subtree_min().
            if (i > 0 && i - 1 < (int)node->keys.size()) {
                int separator = node->keys[i - 1];
                int min_key_child = subtree_min(child);
                if (min_key_child != INT_MAX && separator != min_key_child) {
                    std::cerr << "ERROR: Separator key " << separator 
                              << " doesn't match child minimum " << min_key_child << "\n";
                    return false;
                }
            }
            
            if (!validate_node(child, leaf_depth, current_depth + 1, dfs_leaf_count)) {
                return false;
            }
        }
    }
    
    return true;
}

bool BPlusTree::validate() const {
    // Check 9: Root exists and is correct type
    if (!root) {
        std::cerr << "ERROR: Root is null\n";
        return false;
    }
    
    // BUG FIX #6: Handle empty tree edge case
    if (root->is_leaf && root->keys.empty()) {
        // Valid empty tree
        return true;
    }
    
    // Check 10: Leftmost leaf is correct
    auto node = root;
    while (!node->is_leaf) {
        node = node->children[0];
    }
    if (node != leftmost_leaf) {
        std::cerr << "ERROR: Leftmost leaf pointer is wrong\n";
        return false;
    }
    
    // Check 11: Leaf chain is correct and complete
    // BUG FIX #8: Verify every leaf appears exactly once in the chain
    auto leaf = leftmost_leaf;
    int chain_leaf_count = 1;
    while (leaf != nullptr && leaf->next != nullptr) {
        if (leaf->keys.empty() && leaf->next->keys.empty()) {
            std::cerr << "ERROR: Both consecutive leaves are empty\n";
            return false;
        }
        if (!leaf->keys.empty() && !leaf->next->keys.empty() && leaf->keys.back() >= leaf->next->keys.front()) {
            std::cerr << "ERROR: Leaf chain keys not sorted\n";
            return false;
        }
        leaf = leaf->next;
        chain_leaf_count++;
    }
    
    // Validate entire tree structure
    int leaf_depth = -1;
    int dfs_leaf_count = 0;
    if (!validate_node(root, leaf_depth, 0, dfs_leaf_count)) {
        return false;
    }
    
    // BUG FIX #4: The chain traversal alone can't detect a broken/short-circuited
    // leaf->next pointer (e.g. one skipping over a real leaf) unless we also
    // know how many leaves the tree structurally contains.
    if (chain_leaf_count != dfs_leaf_count) {
        std::cerr << "ERROR: Leaf chain has " << chain_leaf_count
                  << " leaves but tree structure has " << dfs_leaf_count << "\n";
        return false;
    }
    
    return true;
}

void BPlusTree::save_to_file(const std::string& filename) const {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    
    auto leaf = leftmost_leaf;
    while (leaf != nullptr) {
        for (int i = 0; i < leaf->keys.size(); i++) {
            file << leaf->keys[i] << " " << leaf->values[i] << "\n";
        }
        leaf = leaf->next;
    }
    
    file.close();
    std::cout << "Tree saved to " << filename << "\n";
}

void BPlusTree::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    
    int key;
    std::string value;

    while (file >> key) {
        if (file.peek() == ' ') {
            file.get();
        }

        std::getline(file, value);
        insert(key, value);
    }

    if (file.bad()) {
        throw std::runtime_error("Error while reading file: " + filename);
    }
    
    file.close();
    std::cout << "Tree loaded from " << filename << "\n";
}
