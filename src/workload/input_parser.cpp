#include "workload/input_parser.h"
#include "workload/record.h"
#include <fstream>
#include <stdexcept>

namespace txn {

// Parse "{field1: val1, field2: val2, ...}" into a Record.
// Values are either bare integers or double-quoted strings.
static Record ParseValueMap(const std::string& s) {
    Record rec;

    auto open = s.find('{');
    auto close = s.rfind('}');
    if (open == std::string::npos || close == std::string::npos || close <= open)
        return rec;

    std::string inner = s.substr(open + 1, close - open - 1);

    size_t pos = 0;
    while (pos < inner.size()) {
        // Skip leading whitespace
        while (pos < inner.size() && (inner[pos] == ' ' || inner[pos] == '\t')) ++pos;
        if (pos >= inner.size()) break;

        // Read field name up to ':'
        auto colon = inner.find(':', pos);
        if (colon == std::string::npos) break;

        std::string field = inner.substr(pos, colon - pos);
        // Trim trailing spaces from field
        while (!field.empty() && (field.back() == ' ' || field.back() == '\t'))
            field.pop_back();

        pos = colon + 1;
        // Skip whitespace after ':'
        while (pos < inner.size() && inner[pos] == ' ') ++pos;

        std::string value;
        if (pos < inner.size() && inner[pos] == '"') {
            // Quoted string value
            ++pos; // skip opening '"'
            auto end_quote = inner.find('"', pos);
            if (end_quote == std::string::npos) end_quote = inner.size();
            value = inner.substr(pos, end_quote - pos);
            pos = end_quote + 1;
            // Advance past optional comma
            while (pos < inner.size() && inner[pos] != ',') ++pos;
            if (pos < inner.size()) ++pos; // skip ','
        } else {
            // Bare integer value
            auto comma = inner.find(',', pos);
            if (comma == std::string::npos) {
                value = inner.substr(pos);
                pos = inner.size();
            } else {
                value = inner.substr(pos, comma - pos);
                pos = comma + 1;
            }
            // Trim
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
                value.pop_back();
        }

        if (!field.empty()) {
            rec[field] = value;
        }
    }

    return rec;
}

ParseResult ParseInputFile(const std::string& path) {
    ParseResult result;

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open input file: " + path);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line == "INSERT" || line == "END") continue;

        // Expected format: "KEY: <key>, VALUE: <value_map>"
        const std::string key_prefix = "KEY: ";
        const std::string val_sep = ", VALUE: ";

        if (line.rfind(key_prefix, 0) != 0) continue;

        auto sep_pos = line.find(val_sep);
        if (sep_pos == std::string::npos) continue;

        std::string key = line.substr(key_prefix.size(), sep_pos - key_prefix.size());
        std::string value_str = line.substr(sep_pos + val_sep.size());

        Record rec = ParseValueMap(value_str);
        result.initial_data[key] = SerializeRecord(rec);

        // Categorize by key prefix character
        if (key.size() >= 2 && key[1] == '_') {
            char prefix = key[0];
            if (prefix == 'A') {
                result.account_keys.push_back(key);
            } else if (prefix == 'W') {
                result.warehouse_keys.push_back(key);
            } else if (prefix == 'D') {
                result.district_keys.push_back(key);
            } else if (prefix == 'S') {
                result.supply_keys.push_back(key);
            } else if (prefix == 'C') {
                result.customer_keys.push_back(key);
            }
        }
    }

    return result;
}

} // namespace txn
