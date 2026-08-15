#pragma once

#include <string>

namespace bms {

enum class TxType {
    DEPOSIT = 0,
    WITHDRAWAL = 1,
    TRANSFER_OUT = 2,
    TRANSFER_IN = 3,
    INTEREST = 4,
    LOAN_DISBURSAL = 5,
    EMI_PAYMENT = 6
};

// A single financial movement on one account.
struct Transaction {
    long long id = 0;
    std::string accountNumber;
    TxType type = TxType::DEPOSIT;
    long long amountPaise = 0;
    long long balanceAfterPaise = 0;
    std::string counterparty;   // other account / description
    std::string timestamp;      // YYYY-MM-DD HH:MM:SS
    std::string remark;

    std::string typeName() const;
    std::string sign() const;  // "+" or "-"

    // Serialization: one line of '|' separated fields.
    static std::string serialize(const Transaction& tx);
    static bool deserialize(const std::string& line, Transaction& out);
};

}  // namespace bms
