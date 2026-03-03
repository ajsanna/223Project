#ifndef RECORD_H
#define RECORD_H

#include <map>
#include <string>

namespace txn {

using Record = std::map<std::string, std::string>;

// Serialize to pipe-delimited "key=value" pairs, fields sorted for determinism.
// Fields must not contain '|' or '='.
std::string SerializeRecord(const Record& rec);

// Inverse of SerializeRecord.
Record DeserializeRecord(const std::string& s);

// Returns the integer value of field, or 0 if missing/empty.
int GetIntField(const Record& rec, const std::string& field);

// Sets the integer value of field.
void SetIntField(Record& rec, const std::string& field, int v);

} // namespace txn

#endif // RECORD_H
