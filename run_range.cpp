#include "bplus_tree.hpp"
#include <fstream>
#include <iostream>
#include <string>

int main() {
    BPlusTree tree;
    std::ifstream file("students_10000.csv");
    std::string line;
    bool first = true;

    while (std::getline(file, line)) {
        if (first) { first = false; continue; } // skip header
        auto comma = line.find(',');
        int id = std::stoi(line.substr(0, comma));
        tree.insert(id, line.substr(comma + 1));
    }

    auto results = tree.range_scan(100020, 100030);
    for (const auto& row : results) {
        std::cout << row.key << "\t" << row.value << "\n";
    }
}
