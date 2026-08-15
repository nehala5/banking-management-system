#include "services/AccountService.h"

#include <cmath>

#include "core/Utils.h"

namespace bms {

Account* AccountService::openAccount(int customerId, AccountType type,
                                     long long openingDeposit,
                                     std::string& err) {
    if (db_.findCustomerById(customerId) == nullptr) {
        err = "No such customer.";
        return nullptr;
    }
    if (openingDeposit < 0) {
        err = "Opening deposit cannot be negative.";
        return nullptr;
    }

    Account acc;
    acc.accountNumber = db_.nextAccountNumber();
    acc.customerId = customerId;
    acc.type = type;
    acc.balancePaise = openingDeposit;
    acc.interestRate = type == AccountType::SAVINGS ? 3.5 : 0.0;
    acc.status = AccountStatus::ACTIVE;
    acc.createdAt = Utils::now();
    acc.lastInterestApplied = Utils::today();
    db_.accounts().push_back(acc);

    if (openingDeposit > 0) {
        Transaction tx;
        tx.id = db_.nextTransactionId();
        tx.accountNumber = acc.accountNumber;
        tx.type = TxType::DEPOSIT;
        tx.amountPaise = openingDeposit;
        tx.balanceAfterPaise = openingDeposit;
        tx.counterparty = "Bank";
        tx.timestamp = Utils::now();
        tx.remark = "Account opening deposit";
        db_.addTransaction(tx);
    }

    db_.saveAll();
    return &db_.accounts().back();
}

bool AccountService::closeAccount(const std::string& accountNumber,
                                  std::string& err) {
    Account* acc = db_.findAccountByNumber(accountNumber);
    if (!acc) {
        err = "Account not found.";
        return false;
    }
    if (acc->status != AccountStatus::ACTIVE) {
        err = "Account is already closed.";
        return false;
    }
    if (acc->balancePaise != 0) {
        err = "Balance must be zero before closing. Withdraw or transfer first.";
        return false;
    }
    for (const auto& loan : db_.loans()) {
        if (loan.accountNumber == accountNumber &&
            (loan.status == LoanStatus::ACTIVE || loan.status == LoanStatus::PENDING)) {
            err = "Account has an open loan linked to it.";
            return false;
        }
    }

    acc->status = AccountStatus::CLOSED;
    db_.saveAll();
    return true;
}

std::vector<const Account*> AccountService::accountsOfCustomer(int customerId) const {
    std::vector<const Account*> out;
    for (const auto& a : db_.accounts()) {
        if (a.customerId == customerId) out.push_back(&a);
    }
    return out;
}

long long AccountService::applyMonthlyInterest() {
    long long total = 0;
    const std::string today = Utils::today();

    for (auto& acc : db_.accounts()) {
        if (acc.status != AccountStatus::ACTIVE) continue;
        if (acc.interestRate <= 0.0) continue;
        if (acc.lastInterestApplied == today) continue;  // once per day

        long double monthly = acc.interestRate / 100.0L / 12.0L;
        long double gain = static_cast<long double>(acc.balancePaise) * monthly;
        long long gainPaise = std::llround(gain);
        if (gainPaise <= 0) continue;

        acc.balancePaise += gainPaise;
        acc.lastInterestApplied = today;

        Transaction tx;
        tx.id = db_.nextTransactionId();
        tx.accountNumber = acc.accountNumber;
        tx.type = TxType::INTEREST;
        tx.amountPaise = gainPaise;
        tx.balanceAfterPaise = acc.balancePaise;
        tx.counterparty = "Bank";
        tx.timestamp = Utils::now();
        tx.remark = "Monthly interest @ " + std::to_string(acc.interestRate) + "% p.a.";
        db_.addTransaction(tx);

        total += gainPaise;
    }

    if (total > 0) db_.saveAll();
    return total;
}

bool AccountService::setInterestRate(const std::string& accountNumber,
                                     double rate, std::string& err) {
    Account* acc = db_.findAccountByNumber(accountNumber);
    if (!acc) {
        err = "Account not found.";
        return false;
    }
    if (acc->type != AccountType::SAVINGS) {
        err = "Only savings accounts earn interest.";
        return false;
    }
    if (rate < 0.0 || rate > 20.0) {
        err = "Rate must be between 0% and 20%.";
        return false;
    }
    acc->interestRate = rate;
    db_.saveAll();
    return true;
}

}  // namespace bms
