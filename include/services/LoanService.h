#pragma once

#include <string>

#include "core/Database.h"
#include "models/Loan.h"

namespace bms {

// Loan lifecycle: application, sanction, repayment and EMI calculation.
class LoanService {
public:
    explicit LoanService(Database& db) : db_(db) {}

    // EMI for a loan using the standard reducing-balance formula:
    //   EMI = P * r * (1+r)^n / ((1+r)^n - 1),  r = annual rate / 1200.
    static long long computeEmi(long long principalPaise,
                                double ratePerAnnum,
                                int tenureMonths);

    // File a new loan application (status PENDING).
    Loan* applyLoan(int customerId, const std::string& accountNumber,
                    long long principalPaise, double ratePerAnnum,
                    int tenureMonths, std::string& err);

    // Approve a pending loan: computes the EMI and disburses the principal
    // into the linked account.
    bool approveLoan(const std::string& loanId, std::string& err);

    bool rejectLoan(const std::string& loanId, std::string& err);

    // Pay the current EMI from the linked account. Closes the loan once the
    // outstanding principal reaches zero.
    bool payEmi(const std::string& loanId, std::string& err);

    Loan* findOpenLoanByAccount(const std::string& accountNumber);

private:
    Database& db_;
};

}  // namespace bms
