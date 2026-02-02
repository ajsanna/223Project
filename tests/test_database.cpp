#include "database/database.h"
#include <iostream>
#include <cassert>

using namespace txn;

void test_basic_operations() {
    std::cout << "\n=== Testing Basic Operations ===" << std::endl;

    Database db;
    assert(db.Open("test_db"));

    // Clear any existing data
    db.Clear();

    // Test Put and Get
    assert(db.Put("key1", "value1"));
    auto result = db.Get("key1");
    assert(result.has_value());
    assert(result.value() == "value1");
    std::cout << "✓ Put and Get work correctly" << std::endl;

    // Test Get non-existent key
    result = db.Get("nonexistent");
    assert(!result.has_value());
    std::cout << "✓ Get returns empty for non-existent key" << std::endl;

    // Test Update
    assert(db.Put("key1", "value1_updated"));
    result = db.Get("key1");
    assert(result.has_value());
    assert(result.value() == "value1_updated");
    std::cout << "✓ Update works correctly" << std::endl;

    // Test Delete
    assert(db.Delete("key1"));
    result = db.Get("key1");
    assert(!result.has_value());
    std::cout << "✓ Delete works correctly" << std::endl;

    db.Close();
}

void test_initialization() {
    std::cout << "\n=== Testing Database Initialization ===" << std::endl;

    Database db;
    assert(db.Open("test_db"));
    db.Clear();

    // Create initial dataset
    std::map<std::string, std::string> initial_data;
    for (int i = 0; i < 100; i++) {
        initial_data["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    assert(db.InitializeWithData(initial_data));
    assert(db.GetKeyCount() == 100);
    std::cout << "✓ Database initialized with 100 keys" << std::endl;

    // Verify some random keys
    auto result = db.Get("key_42");
    assert(result.has_value());
    assert(result.value() == "value_42");
    std::cout << "✓ Initialized data is accessible" << std::endl;

    db.Close();
}

void test_structured_values() {
    std::cout << "\n=== Testing Structured Values ===" << std::endl;

    Database db;
    assert(db.Open("test_db"));
    db.Clear();

    // Simulate storing a structured object as a string (e.g., JSON)
    // In a real implementation, you might use a serialization library
    std::string user_record = R"({"name":"Alice","balance":1000,"email":"alice@example.com"})";

    assert(db.Put("user:1", user_record));
    auto result = db.Get("user:1");
    assert(result.has_value());
    assert(result.value() == user_record);
    std::cout << "✓ Structured values (JSON-like) can be stored and retrieved" << std::endl;

    db.Close();
}

void test_persistence() {
    std::cout << "\n=== Testing Persistence ===" << std::endl;

    {
        Database db;
        assert(db.Open("test_db"));
        db.Clear();
        assert(db.Put("persistent_key", "persistent_value"));
        db.Close();
    }

    // Reopen database
    {
        Database db;
        assert(db.Open("test_db"));
        auto result = db.Get("persistent_key");
        assert(result.has_value());
        assert(result.value() == "persistent_value");
        std::cout << "✓ Data persists across database sessions" << std::endl;
        db.Close();
    }
}

int main() {
    std::cout << "Starting Database Layer Tests\n" << std::endl;

    try {
        test_basic_operations();
        test_initialization();
        test_structured_values();
        test_persistence();

        std::cout << "\n=== All Tests Passed! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
