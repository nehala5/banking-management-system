#pragma once

#include <string>
#include <vector>

#include "models/Account.h"
#include "models/Customer.h"
#include "models/Loan.h"
#include "models/Transaction.h"

namespace bms {

// File-backed data store. Every model is kept in memory and mirrored to a file
// in the data/ directory. The store also owns the ID / account-number
// sequences and seeds an admin user plus a demo customer on first run.
class Database {
public:
    Database() = default;

    // Load everything from data/ (creating + seeding if absent).
    bool load(const std::string& dataDir);

    // Write all collections back to disk. Returns false on failure.
    bool saveAll() const;

    const std::string& dataDir() const { return dataDir_; }

    // ---- sequences ----
    int nextCustomerId() { return ++customerSeq_; }
    long long nextTransactionId() { return ++txSeq_; }
    std::string nextAccountNumber();
    std::string nextLoanId();

    // ---- collections ----
    std::vector<Customer>& customers() { return customers_; }
    std::vector<Account>& accounts() { return accounts_; }
    std::vector<Transaction>& transactions() { return transactions_; }
    std::vector<Loan>& loans() { return loans_; }

    const std::vector<Customer>& customers() const { return customers_; }
    const std::vector<Account>& accounts() const { return accounts_; }
    const std::vector<Transaction>& transactions() const { return transactions_; }
    const std::vector<Loan>& loans() const { return loans_; }

    // ---- lookups ----
    Customer* findCustomerById(int id);
    const Customer* findCustomerById(int id) const;
    Account* findAccountByNumber(const std::string& number);
    const Account* findAccountByNumber(const std::string& number) const;
    Loan* findLoanById(const std::string& loanId);

    void addTransaction(const Transaction& tx);

private:
    bool ensureDataDir() const;
    bool seedDemoData();

    template <typename T>
    bool saveFile(const std::string& name,
                  const std::vector<T>& rows,
                  std::string (*serialize)(const T&)) const;

    template <typename T>
    bool loadFile(const std::string& name,
                  std::vector<T>& rows,
                  bool (*deserialize)(const std::string&, T&)) const;

    std::string dataDir_;
    int customerSeq_ = 999;
    long long txSeq_ = 0;
    int accountSeq_ = 0;
    int loanSeq_ = 0;

    std::vector<Customer> customers_;
    std::vector<Account> accounts_;
    std::vector<Transaction> transactions_;
    std::vector<Loan> loans_;
};

}  // namespace bms
