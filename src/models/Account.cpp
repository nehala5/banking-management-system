#include "models/Account.h"

#include <sstream>
#include <vector>

namespace bms {

std::string Account::typeName() const {
    return type == AccountType::SAVINGS ? "Savings" : "Current";
}

std::string Account::statusName() const {
    return status == AccountStatus::ACTIVE ? "Active" : "Closed";
}

std::string Account::serialize() const {
    std::ostringstream os;
    os << accountNumber << '|'
       << customerId << '|'
       << static_cast<int>(type) << '|'
       << balancePaise << '|'
       << interestRate << '|'
       << static_cast<int>(status) << '|'
       << createdAt << '|'
       << lastInterestApplied;
    return os.str();
}

bool Account::deserialize(const std::string& line, Account& out) {
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

    if (f.size() != 8) return false;
    try {
        out.accountNumber = f[0];
        out.customerId = std::stoi(f[1]);
        out.type = static_cast<AccountType>(std::stoi(f[2]));
        out.balancePaise = std::stoll(f[3]);
        out.interestRate = std::stod(f[4]);
        out.status = static_cast<AccountStatus>(std::stoi(f[5]));
        out.createdAt = f[6];
        out.lastInterestApplied = f[7];
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace bms
