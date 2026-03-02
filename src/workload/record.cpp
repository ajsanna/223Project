#include "workload/record.h"
#include <sstream>

namespace txn {

std::string SerializeRecord(const Record& rec) {
    std::string result;
    for (const auto& [k, v] : rec) {
        if (!result.empty()) result += '|';
        result += k + '=' + v;
    }
    return result;
}

Record DeserializeRecord(const std::string& s) {
    Record rec;
    if (s.empty()) return rec;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, '|')) {
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            rec[token.substr(0, eq)] = token.substr(eq + 1);
        }
    }
    return rec;
}

int GetIntField(const Record& rec, const std::string& field) {
    auto it = rec.find(field);
    if (it == rec.end() || it->second.empty()) return 0;
    return std::stoi(it->second);
}

void SetIntField(Record& rec, const std::string& field, int v) {
    rec[field] = std::to_string(v);
}

} // namespace txn
