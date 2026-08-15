#include "models/Loan.h"

#include <sstream>
#include <vector>

namespace bms {

std::string Loan::statusName() const {
    switch (status) {
        case LoanStatus::PENDING: return "Pending";
        case LoanStatus::ACTIVE: return "Active";
        case LoanStatus::CLOSED: return "Closed";
        case LoanStatus::REJECTED: return "Rejected";
    }
    return "Unknown";
}

std::string Loan::serialize(const Loan& l) {
    std::ostringstream os;
    os << l.loanId << '|'
       << l.customerId << '|'
       << l.accountNumber << '|'
       << l.principalPaise << '|'
       << l.outstandingPaise << '|'
       << l.ratePerAnnum << '|'
       << l.tenureMonths << '|'
       << l.emiPaise << '|'
       << l.emiPaidCount << '|'
       << l.amountPaidPaise << '|'
       << static_cast<int>(l.status) << '|'
       << l.appliedDate << '|'
       << l.decisionDate << '|'
       << l.nextDueDate;
    return os.str();
}

bool Loan::deserialize(const std::string& line, Loan& out) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == '|') {
            f.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    f.push_back(cur);

    if (f.size() != 14) return false;
    try {
        out.loanId = f[0];
        out.customerId = std::stoi(f[1]);
        out.accountNumber = f[2];
        out.principalPaise = std::stoll(f[3]);
        out.outstandingPaise = std::stoll(f[4]);
        out.ratePerAnnum = std::stod(f[5]);
        out.tenureMonths = std::stoi(f[6]);
        out.emiPaise = std::stoll(f[7]);
        out.emiPaidCount = std::stoi(f[8]);
        out.amountPaidPaise = std::stoll(f[9]);
        out.status = static_cast<LoanStatus>(std::stoi(f[10]));
        out.appliedDate = f[11];
        out.decisionDate = f[12];
        out.nextDueDate = f[13];
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace bms
