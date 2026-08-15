#pragma once

#include <string>

namespace bms {

enum class AccountType { SAVINGS = 0, CURRENT = 1 };
enum class AccountStatus { ACTIVE = 0, CLOSED = 1 };

// A bank account owned by a customer. Balance is stored in paise (1/100th of a
// rupee) as a 64-bit integer to avoid floating-point rounding errors.
struct Account {
    std::string accountNumber;   // e.g. "OB000123"
    int customerId = 0;
    AccountType type = AccountType::SAVINGS;
    long long balancePaise = 0;
    double interestRate = 3.5;   // annual percentage, applied monthly
    AccountStatus status = AccountStatus::ACTIVE;
    std::string createdAt;
    std::string lastInterestApplied;  // YYYY-MM-DD

    std::string typeName() const;
    std::string statusName() const;

    std::string serialize() const;
    static bool deserialize(const std::string& line, Account& out);
};

}  // namespace bms
