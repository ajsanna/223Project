#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <memory>
#include <optional>
#include <map>
#include <rocksdb/db.h>

namespace txn {

/**
 * Database Layer - Wrapper around RocksDB
 * Provides simple key-value storage with Get/Put/Delete operations
 */
class Database {
public:
    /**
     * Opens or creates a database at the specified path
     * @param db_path Path to database directory
     * @return true if successful, false otherwise
     */
    bool Open(const std::string& db_path);

    /**
     * Closes the database
     */
    void Close();

    /**
     * Retrieves a value for a given key
     * @param key The key to look up
     * @return Optional containing the value if found, empty otherwise
     */
    std::optional<std::string> Get(const std::string& key);

    /**
     * Stores a key-value pair
     * @param key The key
     * @param value The value
     * @return true if successful, false otherwise
     */
    bool Put(const std::string& key, const std::string& value);

    /**
     * Deletes a key-value pair
     * @param key The key to delete
     * @return true if successful, false otherwise
     */
    bool Delete(const std::string& key);

    /**
     * Initializes database with preset key-value pairs
     * Useful for setting up initial state before workload execution
     * @param initial_data Map of key-value pairs to insert
     * @return true if all inserts successful, false otherwise
     */
    bool InitializeWithData(const std::map<std::string, std::string>& initial_data);

    /**
     * Clears all data from the database
     * WARNING: This is destructive
     * @return true if successful, false otherwise
     */
    bool Clear();

    /**
     * Gets the total number of keys in the database
     * @return Number of keys
     */
    size_t GetKeyCount();

    /**
     * Checks if database is open
     * @return true if open, false otherwise
     */
    bool IsOpen() const { return db_ != nullptr; }

    // Destructor
    ~Database();

private:
    std::unique_ptr<rocksdb::DB> db_;
    rocksdb::Options options_;
};

} // namespace txn

#endif // DATABASE_H
