#include "database/database.h"
#include <iostream>

using namespace txn;

int main() {
    std::cout << "Transaction Processing System" << std::endl;
    std::cout << "=============================" << std::endl;

    Database db;

    if (!db.Open("transaction_db")) {
        std::cerr << "Failed to open database" << std::endl;
        return 1;
    }

    // Initialize with some sample data
    std::map<std::string, std::string> initial_data;
    for (int i = 0; i < 1000; i++) {
        initial_data["account_" + std::to_string(i)] = std::to_string(1000); // Each account starts with balance 1000
    }

    db.InitializeWithData(initial_data);

    std::cout << "\nDatabase initialized with " << db.GetKeyCount() << " accounts" << std::endl;
    std::cout << "Sample account_0 balance: " << db.Get("account_0").value_or("NOT FOUND") << std::endl;
    std::cout << "Sample account_500 balance: " << db.Get("account_500").value_or("NOT FOUND") << std::endl;

    // TODO: Add terminal UI for transaction commands in Step 3

    db.Close();
    return 0;
}
