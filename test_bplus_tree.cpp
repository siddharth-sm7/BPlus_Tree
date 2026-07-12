#include "bplus_tree.hpp"

#include <chrono>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct StudentRecord {
    int student_id;
    std::string payload;
};

struct StudentPreview {
    std::string first_name;
    std::string last_name;
    std::string program;
    std::string year;
    std::string age;
    std::string city;
    std::string gpa;
};

struct TestRunner {
    int passed = 0;
    int failed = 0;

    void check(bool condition, const std::string& name) {
        if (condition) {
            ++passed;
            std::cout << "[PASS] " << name << "\n";
        } else {
            ++failed;
            std::cout << "[FAIL] " << name << "\n";
        }
    }
};

static std::string trim_cr(std::string value) {
    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
    return value;
}

static bool parse_student_row(const std::string& line, StudentRecord& record) {
    const std::size_t comma = line.find(',');
    if (comma == std::string::npos) {
        return false;
    }

    record.student_id = std::stoi(line.substr(0, comma));
    record.payload = trim_cr(line.substr(comma + 1));
    return true;
}

static std::vector<StudentRecord> load_students_csv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::vector<StudentRecord> rows;
    std::string line;
    bool first_line = true;

    while (std::getline(file, line)) {
        line = trim_cr(line);
        if (line.empty()) {
            continue;
        }

        if (first_line) {
            first_line = false;
            if (line.rfind("student_id,", 0) == 0) {
                continue;
            }
        }

        StudentRecord record{};
        if (!parse_student_row(line, record)) {
            throw std::runtime_error("Malformed CSV row: " + line);
        }
        rows.push_back(std::move(record));
    }

    if (file.bad()) {
        throw std::runtime_error("Error while reading file: " + filename);
    }

    return rows;
}

static std::map<int, std::string> build_reference_map(const std::vector<StudentRecord>& rows) {
    std::map<int, std::string> reference;
    for (const auto& row : rows) {
        reference[row.student_id] = row.payload;
    }
    return reference;
}

static void load_into_tree(const std::vector<StudentRecord>& rows, BPlusTree& tree) {
    for (const auto& row : rows) {
        tree.insert(row.student_id, row.payload);
    }
}

static bool compare_tree_with_map(BPlusTree& tree, const std::map<int, std::string>& reference) {
    std::string value;
    for (const auto& [student_id, expected] : reference) {
        if (!tree.search(student_id, value)) {
            return false;
        }
        if (value != expected) {
            return false;
        }
    }
    return true;
}

static bool compare_range_query(BPlusTree& tree, const std::map<int, std::string>& reference, int min_key, int max_key) {
    auto tree_results = tree.range_scan(min_key, max_key);
    std::vector<std::pair<int, std::string>> map_results;

    auto it = reference.lower_bound(min_key);
    while (it != reference.end() && it->first <= max_key) {
        map_results.emplace_back(it->first, it->second);
        ++it;
    }

    if (tree_results.size() != map_results.size()) {
        return false;
    }

    for (std::size_t index = 0; index < tree_results.size(); ++index) {
        if (tree_results[index].key != map_results[index].first) {
            return false;
        }
        if (tree_results[index].value != map_results[index].second) {
            return false;
        }
    }

    return true;
}

static std::vector<std::string> split_csv_fields(const std::string& text) {
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string field;

    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }

    return fields;
}

static bool parse_student_preview(const std::string& payload, StudentPreview& preview) {
    auto fields = split_csv_fields(payload);
    if (fields.size() < 7) {
        return false;
    }

    preview.first_name = std::move(fields[0]);
    preview.last_name = std::move(fields[1]);
    preview.program = std::move(fields[2]);
    preview.year = std::move(fields[3]);
    preview.age = std::move(fields[4]);
    preview.city = std::move(fields[5]);
    preview.gpa = std::move(fields[6]);
    return true;
}

static void print_tree_sample(BPlusTree& tree, const std::map<int, std::string>& reference, std::size_t limit) {
    if (reference.empty() || limit == 0) {
        return;
    }

    const std::size_t sample_count = std::min(limit, reference.size());
    auto begin_it = reference.begin();
    auto end_it = reference.begin();
    std::advance(end_it, static_cast<std::ptrdiff_t>(sample_count - 1));

    auto sample = tree.range_scan(begin_it->first, end_it->first);

    std::cout << "\nSample stored records (first " << sample_count << " in sorted order)\n";
    std::cout << "-------------------------------------------------------------------------------\n";
    std::cout << std::left
              << std::setw(10) << "ID"
              << std::setw(16) << "Name"
              << std::setw(22) << "Program"
              << std::setw(16) << "City"
              << std::setw(6) << "Yr"
              << std::setw(6) << "Age"
              << std::setw(6) << "GPA"
              << "\n";
    std::cout << "-------------------------------------------------------------------------------\n";
    for (std::size_t index = 0; index < sample.size(); ++index) {
        StudentPreview preview{};
        if (!parse_student_preview(sample[index].value, preview)) {
            std::cout << std::setw(10) << sample[index].key << "<unparsed>\n";
            continue;
        }

        std::cout << std::left
                  << std::setw(10) << sample[index].key
                  << std::setw(16) << (preview.first_name + " " + preview.last_name)
                  << std::setw(22) << preview.program
                  << std::setw(16) << preview.city
                  << std::setw(6) << preview.year
                  << std::setw(6) << preview.age
                  << std::setw(6) << preview.gpa
                  << "\n";
    }
}

static void benchmark_insertions(const std::vector<StudentRecord>& rows) {
    BPlusTree tree;
    std::map<int, std::string> reference;

    auto tree_start = std::chrono::high_resolution_clock::now();
    load_into_tree(rows, tree);
    auto tree_end = std::chrono::high_resolution_clock::now();

    auto map_start = std::chrono::high_resolution_clock::now();
    for (const auto& row : rows) {
        reference[row.student_id] = row.payload;
    }
    auto map_end = std::chrono::high_resolution_clock::now();

    auto tree_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tree_end - tree_start).count();
    auto map_ms = std::chrono::duration_cast<std::chrono::milliseconds>(map_end - map_start).count();

    std::cout << "\nInsertion benchmark\n";
    std::cout << "BPlusTree: " << tree_ms << " ms\n";
    std::cout << "std::map:   " << map_ms << " ms\n";
}

static void benchmark_searches(BPlusTree& tree, const std::map<int, std::string>& reference) {
    std::string value;

    auto tree_start = std::chrono::high_resolution_clock::now();
    for (const auto& [student_id, expected] : reference) {
        if (!tree.search(student_id, value) || value != expected) {
            throw std::runtime_error("Tree search benchmark found a mismatch");
        }
    }
    auto tree_end = std::chrono::high_resolution_clock::now();

    auto map_start = std::chrono::high_resolution_clock::now();
    for (const auto& [student_id, expected] : reference) {
        auto it = reference.find(student_id);
        if (it == reference.end() || it->second != expected) {
            throw std::runtime_error("std::map search benchmark found a mismatch");
        }
    }
    auto map_end = std::chrono::high_resolution_clock::now();

    auto tree_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tree_end - tree_start).count();
    auto map_ms = std::chrono::duration_cast<std::chrono::milliseconds>(map_end - map_start).count();

    std::cout << "\nPoint lookup benchmark\n";
    std::cout << "BPlusTree: " << tree_ms << " ms\n";
    std::cout << "std::map:   " << map_ms << " ms\n";
}

static void benchmark_range_scans(BPlusTree& tree, const std::map<int, std::string>& reference) {
    if (reference.empty()) {
        return;
    }

    const int min_id = reference.begin()->first;
    const int max_id = reference.rbegin()->first;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(min_id, max_id);

    auto tree_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        int left = dist(rng);
        int right = dist(rng);
        if (left > right) {
            std::swap(left, right);
        }
        auto results = tree.range_scan(left, right);
        (void)results;
    }
    auto tree_end = std::chrono::high_resolution_clock::now();

    auto map_start = std::chrono::high_resolution_clock::now();
    rng.seed(12345);
    for (int i = 0; i < 100; ++i) {
        int left = dist(rng);
        int right = dist(rng);
        if (left > right) {
            std::swap(left, right);
        }
        auto it = reference.lower_bound(left);
        while (it != reference.end() && it->first <= right) {
            ++it;
        }
    }
    auto map_end = std::chrono::high_resolution_clock::now();

    auto tree_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tree_end - tree_start).count();
    auto map_ms = std::chrono::duration_cast<std::chrono::milliseconds>(map_end - map_start).count();

    std::cout << "\nRange scan benchmark\n";
    std::cout << "BPlusTree: " << tree_ms << " ms\n";
    std::cout << "std::map:   " << map_ms << " ms\n";
}

int main() {
    const std::string filename = "students_10000.csv";
    TestRunner runner;

    std::cout << "Loading CSV: " << filename << "\n";
    auto rows = load_students_csv(filename);
    auto reference = build_reference_map(rows);

    BPlusTree tree;
    load_into_tree(rows, tree);

    std::cout << "\nTree view\n";
    std::cout << "=========\n";
    tree.print_stats();
    print_tree_sample(tree, reference, 50);

    runner.check(!rows.empty(), "CSV rows loaded");
    runner.check(reference.size() <= rows.size(), "reference map built");
    runner.check(compare_tree_with_map(tree, reference), "BPlusTree matches std::map for all stored rows");
    runner.check(
        reference.empty() || compare_range_query(tree, reference, reference.begin()->first, reference.rbegin()->first),
        "full range scan matches std::map");

    std::cout << "\nRows loaded: " << rows.size() << "\n";
    std::cout << "Unique keys: " << reference.size() << "\n";

    benchmark_insertions(rows);
    benchmark_searches(tree, reference);
    benchmark_range_scans(tree, reference);

    std::cout << "\nPassed: " << runner.passed << "\n";
    std::cout << "Failed: " << runner.failed << "\n";

    return runner.failed == 0 ? 0 : 1;
}
