#ifndef BPLUS_TREE_HPP
#define BPLUS_TREE_HPP

#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <fstream>
#include <climits>

// Different fanouts for internal vs. leaf nodes
const int ORDER_INTERNAL = 256;  // Internal nodes: just pointers, can be large
const int ORDER_LEAF = 64;        // Leaf nodes: stores actual data, must be smaller

struct Entry {
    int key;
    std::string value;
};

class BPlusNode {
public:
    bool is_leaf;
    std::vector<int> keys;
    std::vector<std::string> values;  // Only used in leaf nodes
    std::vector<std::shared_ptr<BPlusNode>> children;  // Only used in internal nodes
    std::shared_ptr<BPlusNode> next;   // For leaf linking (range scan)
    
    BPlusNode(bool is_leaf_node = true) 
        : is_leaf(is_leaf_node), next(nullptr) {}
    
    // Different "full" check for internal vs. leaf
    bool is_full() const {
        if (is_leaf) {
            return keys.size() == ORDER_LEAF - 1;
        } else {
            return keys.size() == ORDER_INTERNAL - 1;
        }
    }
    
    // Different "underfull" check for internal vs. leaf
    bool is_underfull() const {
        if (is_leaf) {
            return keys.size() < (ORDER_LEAF - 1) / 2;
        } else {
            return keys.size() < (ORDER_INTERNAL - 1) / 2;
        }
    }
    
    // Get max keys this node can hold
    int get_max_keys() const {
        return is_leaf ? (ORDER_LEAF - 1) : (ORDER_INTERNAL - 1);
    }
    
    // Get max children this node can have
    int get_max_children() const {
        return is_leaf ? 0 : ORDER_INTERNAL;
    }
};

class BPlusTree {
private:
    std::shared_ptr<BPlusNode> root;
    std::shared_ptr<BPlusNode> leftmost_leaf;
    
    // Helper methods for searching within nodes (binary search)
    int find_child_index(std::shared_ptr<BPlusNode> node, int key);
    int find_child_index_range(std::shared_ptr<BPlusNode> node, int key);
    int find_key_index(std::shared_ptr<BPlusNode> leaf, int key);  // Binary search in leaf
    int find_insert_position(std::shared_ptr<BPlusNode> leaf, int key);  // For inserts
    
    // Split operations with explicit sizing
    void split_root(std::shared_ptr<BPlusNode> full_root);  // Handle root splits separately
    void split_child(std::shared_ptr<BPlusNode> parent, int child_index);
    
    // Rebalancing after deletion
    void merge_or_redistribute_leaf(
        const std::vector<std::shared_ptr<BPlusNode>>& path,
        const std::vector<int>& child_indices);
    void merge_or_redistribute_internal(
        const std::vector<std::shared_ptr<BPlusNode>>& path,
        const std::vector<int>& child_indices,
        size_t node_depth);
    
    // Returns the true minimum key of a subtree by walking to its leftmost leaf.
    // (For a leaf this is just keys.front(); for an internal node its own
    // keys[0] is NOT the subtree minimum, so callers must not substitute it.)
    int subtree_min(std::shared_ptr<BPlusNode> node) const;
    
    // Uses the remembered descent path to fix the single ancestor separator
    // that refers to this subtree, if any.
    void update_separator_upwards(
        const std::vector<std::shared_ptr<BPlusNode>>& path,
        const std::vector<int>& child_indices,
        size_t node_depth,
        int new_min);
    
    // Validation (catches bugs immediately)
    bool validate_node(std::shared_ptr<BPlusNode> node, int& leaf_depth, int current_depth, int& dfs_leaf_count) const;
    
public:
    BPlusTree() {
        root = std::make_shared<BPlusNode>(true);
        leftmost_leaf = root;
    }
    
    void insert(int key, const std::string& value);
    bool search(int key, std::string& value);
    bool remove(int key);
    std::vector<Entry> range_scan(int min_key, int max_key);
    
    // Validation
    bool validate() const;  // Comprehensive tree validation
    
    void print_tree() const;
    void print_leaf_chain() const;
    void print_stats() const;
    
    void save_to_file(const std::string& filename) const;
    void load_from_file(const std::string& filename);
};

#endif
