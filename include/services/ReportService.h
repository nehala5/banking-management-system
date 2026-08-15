#pragma once

#include <ostream>
#include <string>

#include "core/Database.h"

namespace bms {

// Read-only reporting: statements, summaries and bank-wide aggregates.
class ReportService {
public:
    explicit ReportService(const Database& db) : db_(db) {}

    void printAccountStatement(const std::string& accountNumber, int limit,
                               std::ostream& out) const;

    void printCustomerSummary(int customerId, std::ostream& out) const;

    void printBankOverview(std::ostream& out) const;

    void printDailyActivity(std::ostream& out) const;

    void printLoanBook(std::ostream& out) const;

    void printTopAccounts(int n, std::ostream& out) const;

    // Write a statement to a file; returns false on failure.
    bool writeStatementFile(const std::string& accountNumber, int limit,
                            const std::string& path) const;

private:
    const Database& db_;
};

}  // namespace bms
