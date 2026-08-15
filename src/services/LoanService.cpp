#include "services/LoanService.h"

#include <cmath>

#include "core/Utils.h"

namespace bms {

long long LoanService::computeEmi(long long principalPaise,
                                  double ratePerAnnum,
                                  int tenureMonths) {
    if (principalPaise <= 0 || tenureMonths <= 0) return 0;
    const long double p = static_cast<long double>(principalPaise);
    const long double mr = ratePerAnnum / 100.0L / 12.0L;
    if (mr == 0.0L) {
        return std::llround(p / static_cast<long double>(tenureMonths));
    }
    const long double factor = std::pow(1.0L + mr, tenureMonths);
    const long double emi = p * mr * factor / (factor - 1.0L);
    return std::llround(emi);
}

Loan* LoanService::applyLoan(int customerId, const std::string& accountNumber,
                             long long principalPaise, double ratePerAnnum,
                             int tenureMonths, std::string& err) {
    if (db_.findCustomerById(customerId) == nullptr) {
        err = "No such customer.";
        return nullptr;
    }
    const Account* acc = db_.findAccountByNumber(accountNumber);
    if (!acc || acc->customerId != customerId) {
        err = "Loan account does not belong to this customer.";
        return nullptr;
    }
    if (acc->status != AccountStatus::ACTIVE) {
        err = "Loan account is not active.";
        return nullptr;
    }
    if (principalPaise <= 0) {
        err = "Loan amount must be positive.";
        return nullptr;
    }
    if (ratePerAnnum < 0.0 || ratePerAnnum > 36.0) {
        err = "Rate must be between 0% and 36%.";
        return nullptr;
    }
    if (tenureMonths < 1 || tenureMonths > 120) {
        err = "Tenure must be between 1 and 120 months.";
        return nullptr;
    }
    if (findOpenLoanByAccount(accountNumber) != nullptr) {
        err = "An open loan already exists on this account.";
        return nullptr;
    }

    Loan loan;
    loan.loanId = db_.nextLoanId();
    loan.customerId = customerId;
    loan.accountNumber = accountNumber;
    loan.principalPaise = principalPaise;
    loan.outstandingPaise = principalPaise;
    loan.ratePerAnnum = ratePerAnnum;
    loan.tenureMonths = tenureMonths;
    loan.emiPaise = computeEmi(principalPaise, ratePerAnnum, tenureMonths);
    loan.emiPaidCount = 0;
    loan.amountPaidPaise = 0;
    loan.status = LoanStatus::PENDING;
    loan.appliedDate = Utils::now();
    loan.decisionDate = "";
    loan.nextDueDate = "";
    db_.loans().push_back(loan);
    db_.saveAll();
    return &db_.loans().back();
}

bool LoanService::approveLoan(const std::string& loanId, std::string& err) {
    Loan* loan = db_.findLoanById(loanId);
    if (!loan) {
        err = "Loan not found.";
        return false;
    }
    if (loan->status != LoanStatus::PENDING) {
        err = "Loan is not pending approval.";
        return false;
    }

    Account* acc = db_.findAccountByNumber(loan->accountNumber);
    if (!acc || acc->status != AccountStatus::ACTIVE) {
        err = "Linked account is not active.";
        return false;
    }

    loan->status = LoanStatus::ACTIVE;
    loan->emiPaise = computeEmi(loan->principalPaise, loan->ratePerAnnum,
                                loan->tenureMonths);
    loan->decisionDate = Utils::now();
    loan->nextDueDate = Utils::addMonths(Utils::today(), 1);

    acc->balancePaise += loan->principalPaise;

    Transaction tx;
    tx.id = db_.nextTransactionId();
    tx.accountNumber = acc->accountNumber;
    tx.type = TxType::LOAN_DISBURSAL;
    tx.amountPaise = loan->principalPaise;
    tx.balanceAfterPaise = acc->balancePaise;
    tx.counterparty = loan->loanId;
    tx.timestamp = Utils::now();
    tx.remark = "Loan disbursal — " + loan->loanId;
    db_.addTransaction(tx);

    db_.saveAll();
    return true;
}

bool LoanService::rejectLoan(const std::string& loanId, std::string& err) {
    Loan* loan = db_.findLoanById(loanId);
    if (!loan) {
        err = "Loan not found.";
        return false;
    }
    if (loan->status != LoanStatus::PENDING) {
        err = "Loan is not pending.";
        return false;
    }
    loan->status = LoanStatus::REJECTED;
    loan->decisionDate = Utils::now();
    db_.saveAll();
    return true;
}

bool LoanService::payEmi(const std::string& loanId, std::string& err) {
    Loan* loan = db_.findLoanById(loanId);
    if (!loan) {
        err = "Loan not found.";
        return false;
    }
    if (loan->status != LoanStatus::ACTIVE) {
        err = "Loan is not active.";
        return false;
    }

    Account* acc = db_.findAccountByNumber(loan->accountNumber);
    if (!acc || acc->status != AccountStatus::ACTIVE) {
        err = "Linked account is not active.";
        return false;
    }

    const long long due = loan->outstandingPaise < loan->emiPaise
                              ? loan->outstandingPaise
                              : loan->emiPaise;
    if (acc->balancePaise < due) {
        err = "Insufficient balance for this EMI.";
        return false;
    }

    acc->balancePaise -= due;
    loan->amountPaidPaise += due;
    loan->outstandingPaise -= due;
    loan->emiPaidCount += 1;

    if (loan->outstandingPaise <= 0) {
        loan->status = LoanStatus::CLOSED;
        loan->nextDueDate = "";
    } else {
        loan->nextDueDate = Utils::addMonths(loan->nextDueDate, 1);
    }

    Transaction tx;
    tx.id = db_.nextTransactionId();
    tx.accountNumber = acc->accountNumber;
    tx.type = TxType::EMI_PAYMENT;
    tx.amountPaise = due;
    tx.balanceAfterPaise = acc->balancePaise;
    tx.counterparty = loan->loanId;
    tx.timestamp = Utils::now();
    tx.remark = "EMI " + std::to_string(loan->emiPaidCount) + "/" +
                std::to_string(loan->tenureMonths) + " — " + loan->loanId;
    db_.addTransaction(tx);

    db_.saveAll();
    return true;
}

Loan* LoanService::findOpenLoanByAccount(const std::string& accountNumber) {
    for (auto& loan : db_.loans()) {
        if (loan.accountNumber == accountNumber &&
            (loan.status == LoanStatus::ACTIVE ||
             loan.status == LoanStatus::PENDING)) {
            return &loan;
        }
    }
    return nullptr;
}

}  // namespace bms
