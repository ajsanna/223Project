#include "database/database.h"
#include <iostream>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>

namespace txn {

bool Database::Open(const std::string& db_path) {
    // Set RocksDB options
    options_.create_if_missing = true;
    options_.error_if_exists = false;

    // Optimize for better performance
    options_.IncreaseParallelism();
    options_.OptimizeLevelStyleCompaction();

    rocksdb::DB* db_raw;
    rocksdb::Status status = rocksdb::DB::Open(options_, db_path, &db_raw);

    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.ToString() << std::endl;
        return false;
    }

    db_.reset(db_raw);
    std::cout << "Database opened successfully at: " << db_path << std::endl;
    return true;
}

void Database::Close() {
    if (db_) {
        db_.reset();
        std::cout << "Database closed" << std::endl;
    }
}

std::optional<std::string> Database::Get(const std::string& key) {
    if (!db_) {
        std::cerr << "Database not open" << std::endl;
        return std::nullopt;
    }

    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);

    if (status.ok()) {
        return value;
    } else if (status.IsNotFound()) {
        return std::nullopt;
    } else {
        std::cerr << "Get failed: " << status.ToString() << std::endl;
        return std::nullopt;
    }
}

bool Database::Put(const std::string& key, const std::string& value) {
    if (!db_) {
        std::cerr << "Database not open" << std::endl;
        return false;
    }

    rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), key, value);

    if (!status.ok()) {
        std::cerr << "Put failed: " << status.ToString() << std::endl;
        return false;
    }

    return true;
}

bool Database::Delete(const std::string& key) {
    if (!db_) {
        std::cerr << "Database not open" << std::endl;
        return false;
    }

    rocksdb::Status status = db_->Delete(rocksdb::WriteOptions(), key);

    if (!status.ok()) {
        std::cerr << "Delete failed: " << status.ToString() << std::endl;
        return false;
    }

    return true;
}

bool Database::InitializeWithData(const std::map<std::string, std::string>& initial_data) {
    if (!db_) {
        std::cerr << "Database not open" << std::endl;
        return false;
    }

    std::cout << "Initializing database with " << initial_data.size() << " key-value pairs..." << std::endl;

    for (const auto& [key, value] : initial_data) {
        if (!Put(key, value)) {
            std::cerr << "Failed to initialize key: " << key << std::endl;
            return false;
        }
    }

    std::cout << "Database initialization complete" << std::endl;
    return true;
}

bool Database::Clear() {
    if (!db_) {
        std::cerr << "Database not open" << std::endl;
        return false;
    }

    // Use iterator to delete all keys
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        rocksdb::Status status = db_->Delete(rocksdb::WriteOptions(), it->key());
        if (!status.ok()) {
            std::cerr << "Clear failed: " << status.ToString() << std::endl;
            return false;
        }
    }

    if (!it->status().ok()) {
        std::cerr << "Iterator error: " << it->status().ToString() << std::endl;
        return false;
    }

    std::cout << "Database cleared" << std::endl;
    return true;
}

size_t Database::GetKeyCount() {
    if (!db_) {
        return 0;
    }

    size_t count = 0;
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        count++;
    }

    return count;
}

Database::~Database() {
    Close();
}

} // namespace txn
