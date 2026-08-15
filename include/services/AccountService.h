#pragma once

#include <string>
#include <vector>

#include "core/Database.h"
#include "models/Account.h"

namespace bms {

// Account lifecycle and interest logic.
class AccountService {
public:
    explicit AccountService(Database& db) : db_(db) {}

    // Open a new account with an optional opening deposit.
    Account* openAccount(int customerId, AccountType type,
                         long long openingDeposit, std::string& err);

    // Close an account (must have zero balance and no active loans).
    bool closeAccount(const std::string& accountNumber, std::string& err);

    std::vector<const Account*> accountsOfCustomer(int customerId) const;

    // Accrue one month of interest on every active savings account.
    // Returns the total interest credited in paise.
    long long applyMonthlyInterest();

    // Set a new annual interest rate for an account.
    bool setInterestRate(const std::string& accountNumber, double rate,
                         std::string& err);

private:
    Database& db_;
};

}  // namespace bms
