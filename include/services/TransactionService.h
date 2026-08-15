#pragma once

#include <string>
#include <vector>

#include "core/Database.h"
#include "models/Transaction.h"

namespace bms {

// Money movements: deposit, withdraw and transfer, all with validation and a
// full audit trail.
class TransactionService {
public:
    explicit TransactionService(Database& db) : db_(db) {}

    bool deposit(const std::string& accountNumber, long long amountPaise,
                 const std::string& remark, std::string& err);

    bool withdraw(const std::string& accountNumber, long long amountPaise,
                  const std::string& remark, std::string& err);

    // Move funds between two accounts of the bank.
    bool transfer(const std::string& fromNumber, const std::string& toNumber,
                  long long amountPaise, const std::string& remark,
                  std::string& err);

    // Most recent transactions for an account, newest first.
    std::vector<const Transaction*> statementOf(const std::string& accountNumber,
                                                int limit = 50) const;

    // Total volume of transactions performed today (for daily activity reports).
    long long todayVolumePaise() const;

private:
    bool checkSufficientBalance(const Account& acc, long long amountPaise) const;

    Database& db_;
};

}  // namespace bms
