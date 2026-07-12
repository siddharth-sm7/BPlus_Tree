# B+ Tree for MiniDB

v1 is a clean in-memory B+ tree in C++17 for learning, testing, and benchmarking core index behavior.

## v1 Highlights
- Sorted key/value storage in leaf nodes
- Internal nodes for fast navigation
- Linked leaves for range scans
- Insert, search, delete, validate, save, and load

## What v1 Is
- An in-memory B+ tree built with `std::shared_ptr` and `std::vector`
- Good for understanding the data structure and verifying B+ tree operations
- Not a disk-backed storage engine yet

## Build and Run
```bash
make run
```

## v2 Scope
The next version will move from an in-memory model toward a real storage-engine index:
- Fixed-size pages or blocks
- Page IDs instead of direct pointers
- Buffer pool / page cache
- Write-ahead logging and recovery
- Safer split/merge handling for persistent pages
- Better concurrency support

## Files
- `bplus_tree.hpp` - Node and tree declarations
- `bplus_tree.cpp` - B+ tree implementation
- `test_bplus_tree.cpp` - Tests and benchmarks
- `run_range.cpp` - Range scan demo

## Notes
- `ORDER_INTERNAL` and `ORDER_LEAF` control fanout
- Leaf chaining powers efficient range scans
- The current file format is simple and meant for v1 experimentation
