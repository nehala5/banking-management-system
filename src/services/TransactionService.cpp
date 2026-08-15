#include "services/TransactionService.h"

#include "core/Utils.h"
#include "models/Account.h"

namespace bms {

bool TransactionService::deposit(const std::string& accountNumber,
                                 long long amountPaise,
                                 const std::string& remark,
                                 std::string& err) {
    Account* acc = db_.findAccountByNumber(accountNumber);
    if (!acc) {
        err = "Account not found.";
        return false;
    }
    if (acc->status != AccountStatus::ACTIVE) {
        err = "Account is closed.";
        return false;
    }
    if (amountPaise <= 0) {
        err = "Amount must be positive.";
        return false;
    }

    acc->balancePaise += amountPaise;

    Transaction tx;
    tx.id = db_.nextTransactionId();
    tx.accountNumber = accountNumber;
    tx.type = TxType::DEPOSIT;
    tx.amountPaise = amountPaise;
    tx.balanceAfterPaise = acc->balancePaise;
    tx.counterparty = "Bank";
    tx.timestamp = Utils::now();
    tx.remark = remark.empty() ? "Cash deposit" : remark;
    db_.addTransaction(tx);
    db_.saveAll();
    return true;
}

bool TransactionService::withdraw(const std::string& accountNumber,
                                  long long amountPaise,
                                  const std::string& remark,
                                  std::string& err) {
    Account* acc = db_.findAccountByNumber(accountNumber);
    if (!acc) {
        err = "Account not found.";
        return false;
    }
    if (acc->status != AccountStatus::ACTIVE) {
        err = "Account is closed.";
        return false;
    }
    if (amountPaise <= 0) {
        err = "Amount must be positive.";
        return false;
    }
    if (!checkSufficientBalance(*acc, amountPaise)) {
        err = "Insufficient balance.";
        return false;
    }

    acc->balancePaise -= amountPaise;

    Transaction tx;
    tx.id = db_.nextTransactionId();
    tx.accountNumber = accountNumber;
    tx.type = TxType::WITHDRAWAL;
    tx.amountPaise = amountPaise;
    tx.balanceAfterPaise = acc->balancePaise;
    tx.counterparty = "Bank";
    tx.timestamp = Utils::now();
    tx.remark = remark.empty() ? "Cash withdrawal" : remark;
    db_.addTransaction(tx);
    db_.saveAll();
    return true;
}

bool TransactionService::transfer(const std::string& fromNumber,
                                  const std::string& toNumber,
                                  long long amountPaise,
                                  const std::string& remark,
                                  std::string& err) {
    if (fromNumber == toNumber) {
        err = "Cannot transfer to the same account.";
        return false;
    }
    Account* from = db_.findAccountByNumber(fromNumber);
    if (!from || from->status != AccountStatus::ACTIVE) {
        err = "Source account is not active.";
        return false;
    }
    Account* to = db_.findAccountByNumber(toNumber);
    if (!to || to->status != AccountStatus::ACTIVE) {
        err = "Destination account is not active.";
        return false;
    }
    if (amountPaise <= 0) {
        err = "Amount must be positive.";
        return false;
    }
    if (!checkSufficientBalance(*from, amountPaise)) {
        err = "Insufficient balance in the source account.";
        return false;
    }

    from->balancePaise -= amountPaise;
    to->balancePaise += amountPaise;

    const std::string ts = Utils::now();

    Transaction out;
    out.id = db_.nextTransactionId();
    out.accountNumber = fromNumber;
    out.type = TxType::TRANSFER_OUT;
    out.amountPaise = amountPaise;
    out.balanceAfterPaise = from->balancePaise;
    out.counterparty = toNumber;
    out.timestamp = ts;
    out.remark = remark.empty() ? "Funds transfer" : remark;
    db_.addTransaction(out);

    Transaction in;
    in.id = db_.nextTransactionId();
    in.accountNumber = toNumber;
    in.type = TxType::TRANSFER_IN;
    in.amountPaise = amountPaise;
    in.balanceAfterPaise = to->balancePaise;
    in.counterparty = fromNumber;
    in.timestamp = ts;
    in.remark = remark.empty() ? "Funds transfer" : remark;
    db_.addTransaction(in);

    db_.saveAll();
    return true;
}

std::vector<const Transaction*> TransactionService::statementOf(
    const std::string& accountNumber, int limit) const {
    std::vector<const Transaction*> out;
    for (const auto& tx : db_.transactions()) {
        if (tx.accountNumber == accountNumber) out.push_back(&tx);
    }
    if (static_cast<int>(out.size()) > limit) {
        out.erase(out.begin(), out.begin() + (out.size() - limit));
    }
    return out;
}

long long TransactionService::todayVolumePaise() const {
    const std::string today = Utils::today();
    long long volume = 0;
    for (const auto& tx : db_.transactions()) {
        if (tx.timestamp.compare(0, today.size(), today) == 0) {
            volume += tx.amountPaise;
        }
    }
    return volume;
}

bool TransactionService::checkSufficientBalance(const Account& acc,
                                                long long amountPaise) const {
    return acc.balancePaise >= amountPaise;
}

}  // namespace bms
