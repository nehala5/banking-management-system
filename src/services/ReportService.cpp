#include "services/ReportService.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <vector>

#include "core/Utils.h"
#include "models/Account.h"
#include "models/Loan.h"

namespace bms {

namespace {

void drawLine(std::ostream& out, char c = '-', int n = 62) {
    out << std::string(n, c) << "\n";
}

}  // namespace

void ReportService::printAccountStatement(const std::string& accountNumber,
                                          int limit,
                                          std::ostream& out) const {
    const Account* acc = db_.findAccountByNumber(accountNumber);
    if (!acc) {
        out << "Account not found.\n";
        return;
    }

    drawLine(out, '=');
    out << "ACCOUNT STATEMENT\n";
    drawLine(out, '=');
    out << "Account   : " << acc->accountNumber << "  (" << acc->typeName()
        << ")\n";
    out << "Customer  : ";
    const Customer* c = db_.findCustomerById(acc->customerId);
    if (c) out << c->name;
    out << "\n";
    out << "Balance   : " << Utils::formatPaise(acc->balancePaise) << "\n";
    drawLine(out, '-');
    out << "  #  | Date                | Type         | Amount       | "
           "Balance      | Remark\n";
    drawLine(out, '-');

    std::vector<const Transaction*> txs;
    for (const auto& tx : db_.transactions()) {
        if (tx.accountNumber == accountNumber) txs.push_back(&tx);
    }
    if (static_cast<int>(txs.size()) > limit) {
        txs.erase(txs.begin(), txs.begin() + (txs.size() - limit));
    }

    for (const auto* tx : txs) {
        const std::string amount =
            tx->sign() + Utils::formatPaisePlain(tx->amountPaise);
        out << std::setw(4) << tx->id << " | " << std::left << std::setw(19)
            << tx->timestamp << std::right << std::setw(13) << tx->typeName()
            << " | " << std::setw(14) << std::left << amount << std::right
            << " | " << std::setw(14) << std::left
            << Utils::formatPaise(tx->balanceAfterPaise) << std::right << " | "
            << tx->remark << "\n";
    }
    drawLine(out, '-');
    out << txs.size() << " transaction(s) shown. Closing balance: "
        << Utils::formatPaise(acc->balancePaise) << "\n";
}

void ReportService::printCustomerSummary(int customerId, std::ostream& out) const {
    const Customer* c = db_.findCustomerById(customerId);
    if (!c) {
        out << "Customer not found.\n";
        return;
    }

    drawLine(out, '=');
    out << "CUSTOMER SUMMARY — " << c->name << " (ID " << c->id << ")\n";
    drawLine(out, '=');
    out << "Email   : " << c->email << "\n";
    out << "Phone   : " << c->phone << "\n";
    out << "Address : " << c->address << "\n";
    out << "Opened  : " << c->createdAt << "\n";
    drawLine(out, '-');

    long long totalBalance = 0;
    int activeLoans = 0;
    long long totalLoanOutstanding = 0;

    out << "ACCOUNTS\n";
    for (const auto& a : db_.accounts()) {
        if (a.customerId != customerId) continue;
        out << "  " << a.accountNumber << "  " << a.typeName()
            << "  Balance: " << Utils::formatPaise(a.balancePaise)
            << "  [" << a.statusName() << "]\n";
        totalBalance += a.balancePaise;
    }
    drawLine(out, '-');
    out << "LOANS\n";
    for (const auto& l : db_.loans()) {
        if (l.customerId != customerId) continue;
        out << "  " << l.loanId << "  " << Utils::formatPaise(l.outstandingPaise)
            << " outstanding / " << Utils::formatPaise(l.principalPaise)
            << " principal  " << l.statusName() << "  EMI "
            << Utils::formatPaise(l.emiPaise) << "\n";
        if (l.status == LoanStatus::ACTIVE) {
            ++activeLoans;
            totalLoanOutstanding += l.outstandingPaise;
        }
    }
    drawLine(out, '=');
    out << "Total balances      : " << Utils::formatPaise(totalBalance) << "\n";
    out << "Active loans        : " << activeLoans << "\n";
    out << "Loan outstanding    : " << Utils::formatPaise(totalLoanOutstanding)
        << "\n";
}

void ReportService::printBankOverview(std::ostream& out) const {
    long long customerDeposits = 0;
    int activeAccounts = 0;
    for (const auto& a : db_.accounts()) {
        if (a.status != AccountStatus::ACTIVE) continue;
        ++activeAccounts;
        customerDeposits += a.balancePaise;
    }

    long long loanBook = 0;
    int activeLoans = 0;
    for (const auto& l : db_.loans()) {
        if (l.status == LoanStatus::ACTIVE) {
            ++activeLoans;
            loanBook += l.outstandingPaise;
        }
    }

    long long txVolume = 0;
    int txCount = 0;
    for (const auto& tx : db_.transactions()) {
        txVolume += tx.amountPaise;
        ++txCount;
    }

    int customerCount = 0;
    for (const auto& c : db_.customers())
        if (c.role == Role::CUSTOMER) ++customerCount;

    drawLine(out, '=');
    out << "BANK-WIDE OVERVIEW\n";
    drawLine(out, '=');
    out << "Registered customers : " << customerCount << "\n";
    out << "Active accounts      : " << activeAccounts << "\n";
    out << "Customer deposits    : " << Utils::formatPaise(customerDeposits)
        << "\n";
    out << "Active loans         : " << activeLoans << "  (book "
        << Utils::formatPaise(loanBook) << ")\n";
    out << "Loan-to-deposit ratio: "
        << (customerDeposits > 0
                ? std::to_string(
                      static_cast<long long>(
                          static_cast<long double>(loanBook) * 100.0L /
                          static_cast<long double>(customerDeposits)))
                      .substr(0, 4) + "%"
                : "n/a")
        << "\n";
    out << "Total transactions   : " << txCount
        << "  (lifetime volume " << Utils::formatPaise(txVolume) << ")\n";
    drawLine(out, '=');
}

void ReportService::printDailyActivity(std::ostream& out) const {
    const std::string today = Utils::today();
    int count = 0;
    long long volume = 0;
    int deposits = 0, withdrawals = 0, transfers = 0, other = 0;

    for (const auto& tx : db_.transactions()) {
        if (tx.timestamp.compare(0, today.size(), today) != 0) continue;
        ++count;
        volume += tx.amountPaise;
        switch (tx.type) {
            case TxType::DEPOSIT: ++deposits; break;
            case TxType::WITHDRAWAL: ++withdrawals; break;
            case TxType::TRANSFER_IN:
            case TxType::TRANSFER_OUT: ++transfers; break;
            default: ++other; break;
        }
    }

    drawLine(out, '=');
    out << "DAILY ACTIVITY — " << today << "\n";
    drawLine(out, '=');
    out << "Transactions : " << count << "\n";
    out << "Volume       : " << Utils::formatPaise(volume) << "\n";
    out << "Breakdown    : " << deposits << " deposit(s), " << withdrawals
        << " withdrawal(s), " << transfers << " transfer(s), " << other
        << " other\n";
    drawLine(out, '=');
}

void ReportService::printLoanBook(std::ostream& out) const {
    drawLine(out, '=');
    out << "LOAN BOOK\n";
    drawLine(out, '=');
    out << "Loan ID | Customer | Principal    | Outstanding  | EMI        | "
           "Paid  | Status\n";
    drawLine(out, '-');
    for (const auto& l : db_.loans()) {
        out << std::left << std::setw(8) << l.loanId << std::setw(10)
            << l.customerId << std::right
            << Utils::formatPaisePlain(l.principalPaise) << "  "
            << Utils::formatPaisePlain(l.outstandingPaise) << "  "
            << Utils::formatPaisePlain(l.emiPaise) << "  " << std::setw(5)
            << l.emiPaidCount << " " << l.statusName() << "\n";
    }
    drawLine(out, '=');
}

void ReportService::printTopAccounts(int n, std::ostream& out) const {
    std::vector<const Account*> all;
    for (const auto& a : db_.accounts())
        if (a.status == AccountStatus::ACTIVE) all.push_back(&a);
    std::sort(all.begin(), all.end(),
              [](const Account* a, const Account* b) {
                  return a->balancePaise > b->balancePaise;
              });

    drawLine(out, '=');
    out << "TOP ACCOUNTS BY BALANCE\n";
    drawLine(out, '=');
    out << "Rank | Account   | Owner           | Balance\n";
    drawLine(out, '-');
    int shown = 0;
    for (const Account* a : all) {
        if (shown++ >= n) break;
        const Customer* c = db_.findCustomerById(a->customerId);
        out << std::setw(4) << shown << " | " << std::left << std::setw(10)
            << a->accountNumber << std::setw(16)
            << (c ? c->name : "?") << std::right
            << Utils::formatPaise(a->balancePaise) << "\n";
    }
    drawLine(out, '=');
}

bool ReportService::writeStatementFile(const std::string& accountNumber,
                                       int limit,
                                       const std::string& path) const {
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;
    printAccountStatement(accountNumber, limit, file);
    return static_cast<bool>(file);
}

}  // namespace bms
