#pragma once

#include <string>

namespace bms {

enum class LoanStatus { PENDING = 0, ACTIVE = 1, CLOSED = 2, REJECTED = 3 };

// A consumer loan. EMI is computed at sanction time using the standard
// reducing-balance formula; the outstanding principal is tracked in paise.
struct Loan {
    std::string loanId;          // e.g. "LN0001"
    int customerId = 0;
    std::string accountNumber;   // repayment source
    long long principalPaise = 0;
    long long outstandingPaise = 0;
    double ratePerAnnum = 0.0;
    int tenureMonths = 0;
    long long emiPaise = 0;
    int emiPaidCount = 0;
    long long amountPaidPaise = 0;
    LoanStatus status = LoanStatus::PENDING;
    std::string appliedDate;
    std::string decisionDate;    // sanctioned or rejected
    std::string nextDueDate;

    std::string statusName() const;

    static std::string serialize(const Loan& l);
    static bool deserialize(const std::string& line, Loan& out);
};

}  // namespace bms
