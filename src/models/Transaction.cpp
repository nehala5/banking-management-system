#include "models/Transaction.h"

#include <sstream>
#include <vector>

namespace bms {

std::string Transaction::typeName() const {
    switch (type) {
        case TxType::DEPOSIT: return "DEPOSIT";
        case TxType::WITHDRAWAL: return "WITHDRAWAL";
        case TxType::TRANSFER_OUT: return "TRANSFER OUT";
        case TxType::TRANSFER_IN: return "TRANSFER IN";
        case TxType::INTEREST: return "INTEREST";
        case TxType::LOAN_DISBURSAL: return "LOAN DISBURSAL";
        case TxType::EMI_PAYMENT: return "EMI PAYMENT";
    }
    return "UNKNOWN";
}

std::string Transaction::sign() const {
    switch (type) {
        case TxType::DEPOSIT:
        case TxType::TRANSFER_IN:
        case TxType::INTEREST:
        case TxType::LOAN_DISBURSAL:
            return "+";
        default:
            return "-";
    }
}

std::string Transaction::serialize() const {
    std::ostringstream os;
    os << id << '|'
       << accountNumber << '|'
       << static_cast<int>(type) << '|'
       << amountPaise << '|'
       << balanceAfterPaise << '|'
       << counterparty << '|'
       << timestamp << '|'
       << remark;
    return os.str();
}

bool Transaction::deserialize(const std::string& line, Transaction& out) {
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
        out.id = std::stoll(f[0]);
        out.accountNumber = f[1];
        out.type = static_cast<TxType>(std::stoi(f[2]));
        out.amountPaise = std::stoll(f[3]);
        out.balanceAfterPaise = std::stoll(f[4]);
        out.counterparty = f[5];
        out.timestamp = f[6];
        out.remark = f[7];
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace bms
