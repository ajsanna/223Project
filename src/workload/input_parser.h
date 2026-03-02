#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <map>
#include <string>
#include <vector>

namespace txn {

struct ParseResult {
    std::map<std::string, std::string> initial_data;
    std::vector<std::string> account_keys;    // A_*
    std::vector<std::string> warehouse_keys;  // W_*
    std::vector<std::string> district_keys;   // D_*
    std::vector<std::string> supply_keys;     // S_*
    std::vector<std::string> customer_keys;   // C_*
};

// Parses workloads/workload*/input*.txt.
// Format: INSERT / KEY: X, VALUE: {...} lines / END
// Converts multi-field values to SerializeRecord() strings for storage.
ParseResult ParseInputFile(const std::string& path);

} // namespace txn

#endif // INPUT_PARSER_H
