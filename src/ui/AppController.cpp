#include "ui/AppController.h"

#include <fstream>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "core/Utils.h"
#include "models/Loan.h"
#include "ui/Menu.h"
#include "ui/Validator.h"

namespace bms {

AppController::AppController()
    : auth_(db_),
      accountSvc_(db_),
      txSvc_(db_),
      loanSvc_(db_),
      reports_(db_) {}

void AppController::run() {
    const std::string dataDir = "data";
    if (!db_.load(dataDir)) {
        Menu::showError("Could not initialise the data directory ('" + dataDir +
                        "').");
        return;
    }
    welcome();
    authMenu();
}

void AppController::welcome() {
    Menu::clear();
    Menu::header("ONESKY BANK");
    std::cout << "\n  " << Utils::greeting()
              << "! Welcome to the OneSky Bank Management System.\n";
    std::cout << "  A console application written in C++ with file-based\n";
    std::cout << "  persistence, account, transaction and loan management.\n\n";
    Menu::pause();
}

void AppController::authMenu() {
    while (true) {
        Menu::header("Sign in or register");
        std::cout << "  First run? An admin account is pre-seeded:\n";
        std::cout << "    Admin  -> ID 1000, PIN 1234\n";
        std::cout << "    Demo   -> ID 1001, PIN 0000\n\n";
        Menu::options({"Login", "Register as new customer", "Exit"});

        const int c = Menu::choice(1, 3);
        if (c == 1) {
            Menu::header("Login");
            const int id =
                Validator::readInt("  User ID: ", 1, 100000000);
            const std::string pin = Validator::readPin("  PIN: ");
            Customer* cust = auth_.authenticate(id, pin);
            if (!cust) {
                Menu::showError("Invalid user ID or PIN.");
                Menu::pause();
                continue;
            }
            if (cust->role == Role::ADMIN) {
                adminMenu(*cust);
            } else {
                customerMenu(*cust);
            }
        } else if (c == 2) {
            Menu::header("New customer registration");
            const std::string name = Validator::readName("  Full name: ");
            const std::string email = Validator::readEmail("  Email: ");
            if (auth_.emailExists(email)) {
                Menu::showError("That email is already registered.");
                Menu::pause();
                continue;
            }
            const std::string phone = Validator::readPhone("  Phone (10 digits): ");
            const std::string address =
                Validator::readText("  Address: ");
            const std::string pin = Validator::readPin("  Choose a 4-digit PIN: ");
            const std::string pin2 = Validator::readPin("  Confirm PIN: ");
            if (pin != pin2) {
                Menu::showError("PINs do not match.");
                Menu::pause();
                continue;
            }

            Customer* created =
                auth_.registerCustomer(name, email, phone, address, pin);
            if (!created) {
                Menu::showError("Registration failed.");
                Menu::pause();
                continue;
            }
            Menu::showSuccess("Welcome to OneSky Bank, " + created->name +
                              "! Your user ID is " +
                              std::to_string(created->id) +
                              ". A savings account has been opened for you.");
            Menu::pause();
            customerMenu(*created);
        } else {
            Menu::header("Goodbye");
            std::cout << "  Thank you for using OneSky Bank. Stay financially "
                         "sunny!\n\n";
            return;
        }
    }
}

void AppController::adminMenu(Customer& admin) {
    while (true) {
        Menu::header("Administrator Panel");
        std::cout << "  Logged in: " << admin.name << " (ID " << admin.id
                  << ")\n\n";
        Menu::options({"Search customer",
                       "List all customers",
                       "Open a new account",
                       "Close an account",
                       "Set savings interest rate",
                       "Pending loan applications",
                       "Apply monthly interest to savings",
                       "Reports",
                       "Logout"});
        const int c = Menu::choice(0, 9);
        switch (c) {
            case 1: searchCustomersFlow(); break;
            case 2: listCustomersFlow(); break;
            case 3: openAccountFlow(); break;
            case 4: closeAccountFlow(); break;
            case 5: setRateFlow(); break;
            case 6: loanApprovalsFlow(); break;
            case 7: {
                const long long total = accountSvc_.applyMonthlyInterest();
                Menu::showSuccess("Interest credited on all eligible savings "
                                  "accounts — total " +
                                  Utils::formatPaise(total) + ".");
                Menu::pause();
                break;
            }
            case 8: reportsMenu(); break;
            default: return;
        }
    }
}

void AppController::customerMenu(Customer& cust) {
    while (true) {
        Menu::header("Customer Portal");
        std::cout << "  " << Utils::greeting() << ", " << cust.name
                  << " (ID " << cust.id << ")\n\n";
        Menu::options({"My accounts",
                       "Deposit money",
                       "Withdraw money",
                       "Transfer money",
                       "Transaction history",
                       "Loans",
                       "My profile",
                       "Download statement",
                       "Logout"});
        const int c = Menu::choice(0, 9);
        switch (c) {
            case 1: {
                const auto accounts = accountSvc_.accountsOfCustomer(cust.id);
                Menu::header("My accounts");
                if (accounts.empty()) {
                    Menu::showError("You have no accounts yet. Contact the "
                                    "bank administrator.");
                } else {
                    for (const auto* a : accounts) printAccountCard(*a);
                }
                Menu::pause();
                break;
            }
            case 2: depositFlow(cust); break;
            case 3: withdrawFlow(cust); break;
            case 4: transferFlow(cust); break;
            case 5: historyFlow(cust); break;
            case 6: customerLoansMenu(cust); break;
            case 7: {
                Menu::header("My profile");
                reports_.printCustomerSummary(cust.id, std::cout);
                Menu::pause();
                break;
            }
            case 8: downloadStatementFlow(cust); break;
            default: return;
        }
    }
}

void AppController::printAccountCard(const Account& acc) const {
    std::cout << "\n  --------------------------------------------\n";
    std::cout << "  Account   : " << acc.accountNumber << "\n";
    std::cout << "  Type      : " << acc.typeName() << "\n";
    std::cout << "  Status    : " << acc.statusName() << "\n";
    std::cout << "  Rate      : " << acc.interestRate << "% p.a.\n";
    std::cout << "  Balance   : " << Utils::formatPaise(acc.balancePaise)
              << "\n";
    std::cout << "  Opened    : " << acc.createdAt << "\n";
    std::cout << "  --------------------------------------------\n";
}

Account* AppController::pickOwnAccount(const Customer& cust) {
    const auto accounts = accountSvc_.accountsOfCustomer(cust.id);
    std::vector<const Account*> active;
    for (const auto* a : accounts) {
        if (a->status == AccountStatus::ACTIVE) active.push_back(a);
    }
    if (active.empty()) {
        Menu::showError("You have no active accounts.");
        Menu::pause();
        return nullptr;
    }

    std::cout << "\n  Choose an account:\n";
    for (size_t i = 0; i < active.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << active[i]->accountNumber
                  << "  (" << active[i]->typeName() << ")  "
                  << Utils::formatPaise(active[i]->balancePaise) << "\n";
    }
    std::cout << "  [0] Cancel\n";
    const int pick = Menu::choice(0, static_cast<int>(active.size()));
    if (pick == 0) return nullptr;
    return db_.findAccountByNumber(active[pick - 1]->accountNumber);
}

void AppController::depositFlow(Customer& cust) {
    Menu::header("Deposit money");
    Account* acc = pickOwnAccount(cust);
    if (!acc) return;
    const long long amount =
        Validator::readPaise("  Amount to deposit: ", 1);
    const std::string remark =
        Validator::readText("  Remark (optional): ");
    std::string err;
    if (txSvc_.deposit(acc->accountNumber, amount, remark, err)) {
        Menu::showSuccess("Deposited " + Utils::formatPaise(amount) +
                          " to " + acc->accountNumber +
                          ". New balance: " +
                          Utils::formatPaise(acc->balancePaise) + ".");
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::withdrawFlow(Customer& cust) {
    Menu::header("Withdraw money");
    Account* acc = pickOwnAccount(cust);
    if (!acc) return;
    std::cout << "  Available balance: "
              << Utils::formatPaise(acc->balancePaise) << "\n";
    const long long amount =
        Validator::readPaise("  Amount to withdraw: ", 1);
    const std::string remark =
        Validator::readText("  Remark (optional): ");
    std::string err;
    if (txSvc_.withdraw(acc->accountNumber, amount, remark, err)) {
        Menu::showSuccess("Withdrew " + Utils::formatPaise(amount) +
                          " from " + acc->accountNumber +
                          ". New balance: " +
                          Utils::formatPaise(acc->balancePaise) + ".");
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::transferFlow(Customer& cust) {
    Menu::header("Transfer money");
    Account* from = pickOwnAccount(cust);
    if (!from) return;
    const std::string toNumber =
        Validator::readText("  Beneficiary account number: ");
    const Account* to = db_.findAccountByNumber(toNumber);
    if (!to) {
        Menu::showError("Destination account not found.");
        Menu::pause();
        return;
    }
    std::cout << "  Beneficiary: ";
    const Customer* bc = db_.findCustomerById(to->customerId);
    if (bc) std::cout << bc->name;
    std::cout << "  (" << to->accountNumber << ")\n";
    const long long amount = Validator::readPaise("  Amount: ", 1);
    const std::string remark =
        Validator::readText("  Remark (optional): ");
    if (!Validator::confirm("  Confirm transfer?")) {
        Menu::showError("Transfer cancelled.");
        Menu::pause();
        return;
    }
    std::string err;
    if (txSvc_.transfer(from->accountNumber, toNumber, amount, remark, err)) {
        Menu::showSuccess("Transferred " + Utils::formatPaise(amount) +
                          " from " + from->accountNumber + " to " + toNumber +
                          ".");
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::historyFlow(Customer& cust) {
    Menu::header("Transaction history");
    Account* acc = pickOwnAccount(cust);
    if (!acc) return;
    Menu::clear();
    reports_.printAccountStatement(acc->accountNumber, 25, std::cout);
    Menu::pause();
}

void AppController::customerLoansMenu(Customer& cust) {
    while (true) {
        Menu::header("Loans");
        Menu::options({"Apply for a loan",
                       "My loans",
                       "Pay an EMI",
                       "Back to portal"});
        const int c = Menu::choice(1, 4);
        if (c == 1) {
            Menu::header("Apply for a loan");
            Account* acc = pickOwnAccount(cust);
            if (!acc) continue;
            const long long principal =
                Validator::readPaise("  Loan amount (min 10,000): ", 1000000);
            const double rate = Validator::readDouble(
                "  Annual interest rate % (5 - 36): ", 5.0, 36.0);
            const int tenure =
                Validator::readInt("  Tenure in months (1 - 120): ", 1, 120);
            const long long emi =
                LoanService::computeEmi(principal, rate, tenure);
            std::cout << "  Estimated EMI: " << Utils::formatPaise(emi)
                      << " / month\n";
            if (!Validator::confirm("  Submit application?")) continue;
            std::string err;
            Loan* loan = loanSvc_.applyLoan(cust.id, acc->accountNumber,
                                            principal, rate, tenure, err);
            if (!loan) {
                Menu::showError(err);
            } else {
                Menu::showSuccess("Application " + loan->loanId +
                                  " submitted. Awaiting admin approval.");
            }
            Menu::pause();
        } else if (c == 2) {
            Menu::header("My loans");
            bool any = false;
            for (const auto& l : db_.loans()) {
                if (l.customerId != cust.id) continue;
                any = true;
                std::cout << "\n  " << l.loanId << "  " << l.statusName()
                          << "\n";
                std::cout << "    Principal  : " << Utils::formatPaise(l.principalPaise)
                          << "\n";
                std::cout << "    Outstanding: " << Utils::formatPaise(l.outstandingPaise)
                          << "\n";
                std::cout << "    EMI        : " << Utils::formatPaise(l.emiPaise)
                          << " (" << l.ratePerAnnum << "% p.a., "
                          << l.tenureMonths << " months)\n";
                std::cout << "    Paid       : " << l.emiPaidCount << "/"
                          << l.tenureMonths << " installments";
                if (l.status == LoanStatus::ACTIVE) {
                    std::cout << "\n    Next due   : " << l.nextDueDate;
                }
                std::cout << "\n";
            }
            if (!any) Menu::showError("You have no loan records.");
            Menu::pause();
        } else if (c == 3) {
            Menu::header("Pay an EMI");
            std::vector<const Loan*> active;
            for (const auto& l : db_.loans()) {
                if (l.customerId == cust.id &&
                    l.status == LoanStatus::ACTIVE) {
                    active.push_back(&l);
                }
            }
            if (active.empty()) {
                Menu::showError("You have no active loans.");
                Menu::pause();
                continue;
            }
            std::cout << "\n  Active loans:\n";
            for (size_t i = 0; i < active.size(); ++i) {
                std::cout << "  [" << (i + 1) << "] " << active[i]->loanId
                          << "  EMI " << Utils::formatPaise(active[i]->emiPaise)
                          << "  due " << active[i]->nextDueDate << "\n";
            }
            std::cout << "  [0] Cancel\n";
            const int pick = Menu::choice(0, static_cast<int>(active.size()));
            if (pick == 0) continue;
            std::string err;
            if (loanSvc_.payEmi(active[pick - 1]->loanId, err)) {
                Menu::showSuccess("EMI paid on " + active[pick - 1]->loanId +
                                  ".");
            } else {
                Menu::showError(err);
            }
            Menu::pause();
        } else {
            return;
        }
    }
}

void AppController::downloadStatementFlow(Customer& cust) {
    Menu::header("Download statement");
    Account* acc = pickOwnAccount(cust);
    if (!acc) return;
    const std::string path = "data/statements/" + acc->accountNumber + "-" +
                             Utils::today() + ".txt";
#ifdef _WIN32
    _mkdir("data/statements");
#else
    mkdir("data/statements", 0755);
#endif
    if (reports_.writeStatementFile(acc->accountNumber, 100, path)) {
        Menu::showSuccess("Statement saved to " + path + ".");
    } else {
        Menu::showError("Could not write the statement file.");
    }
    Menu::pause();
}

void AppController::searchCustomersFlow() {
    Menu::header("Search customers");
    const std::string term =
        Validator::readText("  Search by ID or name: ");
    std::vector<const Customer*> matches;
    for (const auto& c : db_.customers()) {
        const bool byId = Utils::isNumeric(term) &&
                          std::to_string(c.id) == term;
        const bool byName =
            Utils::lower(c.name).find(Utils::lower(term)) != std::string::npos;
        if (byId || byName) matches.push_back(&c);
    }
    if (matches.empty()) {
        Menu::showError("No customers match '" + term + "'.");
        Menu::pause();
        return;
    }
    for (const auto* c : matches) {
        reports_.printCustomerSummary(c->id, std::cout);
    }
    Menu::pause();
}

void AppController::listCustomersFlow() {
    Menu::header("All customers");
    for (const auto& c : db_.customers()) {
        std::cout << "  " << std::to_string(c.id) << "  " << c.name << "  "
                  << c.email << "  " << c.phone << "  " << c.roleName()
                  << "\n";
    }
    Menu::pause();
}

void AppController::openAccountFlow() {
    Menu::header("Open a new account");
    const int customerId =
        Validator::readInt("  Customer ID: ", 1, 100000000);
    if (!db_.findCustomerById(customerId)) {
        Menu::showError("Customer not found.");
        Menu::pause();
        return;
    }
    std::cout << "  Account type:\n";
    Menu::options({"Savings", "Current"});
    const int typePick = Menu::choice(1, 2);
    const AccountType type = typePick == 1 ? AccountType::SAVINGS
                                           : AccountType::CURRENT;
    const long long deposit =
        Validator::readPaise("  Opening deposit (0 allowed): ", 0);
    std::string err;
    Account* acc = accountSvc_.openAccount(customerId, type, deposit, err);
    if (acc) {
        Menu::showSuccess("Opened " + acc->typeName() + " account " +
                          acc->accountNumber + ".");
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::closeAccountFlow() {
    Menu::header("Close an account");
    const std::string number =
        Validator::readText("  Account number: ");
    std::string err;
    if (accountSvc_.closeAccount(number, err)) {
        Menu::showSuccess("Account " + number + " closed.");
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::setRateFlow() {
    Menu::header("Set savings interest rate");
    const std::string number =
        Validator::readText("  Account number: ");
    const double rate =
        Validator::readDouble("  Annual rate % (0 - 20): ", 0.0, 20.0);
    std::string err;
    if (accountSvc_.setInterestRate(number, rate, err)) {
        Menu::showSuccess("Interest rate set to " + std::to_string(rate) +
                          "% on " + number + ".");
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::loanApprovalsFlow() {
    Menu::header("Pending loan applications");
    std::vector<Loan*> pending;
    for (auto& l : db_.loans()) {
        if (l.status == LoanStatus::PENDING) pending.push_back(&l);
    }
    if (pending.empty()) {
        Menu::showError("No pending applications.");
        Menu::pause();
        return;
    }
    for (const auto* l : pending) {
        const Customer* c = db_.findCustomerById(l->customerId);
        std::cout << "\n  " << l->loanId << "  "
                  << Utils::formatPaise(l->principalPaise) << "  requested by ";
        if (c) std::cout << c->name << " (ID " << c->id << ")";
        std::cout << "\n    Rate " << l->ratePerAnnum << "% p.a.  Tenure "
                  << l->tenureMonths << " months  EMI "
                  << Utils::formatPaise(l->emiPaise) << "\n";
    }
    std::cout << "\n  Enter a loan ID to review (or press Enter to go back): ";
    std::string loanId;
    std::getline(std::cin, loanId);
    loanId = Utils::trim(loanId);
    if (loanId.empty()) return;

    Loan* target = db_.findLoanById(loanId);
    if (!target || target->status != LoanStatus::PENDING) {
        Menu::showError("No pending loan with ID " + loanId + ".");
        Menu::pause();
        return;
    }
    std::cout << "\n  Approve or reject " << loanId << "?\n";
    Menu::options({"Approve and disburse", "Reject"});
    const int c = Menu::choice(1, 2);
    std::string err;
    const bool ok = c == 1 ? loanSvc_.approveLoan(loanId, err)
                           : loanSvc_.rejectLoan(loanId, err);
    if (ok) {
        Menu::showSuccess(loanId + (c == 1 ? " approved and disbursed."
                                           : " rejected."));
    } else {
        Menu::showError(err);
    }
    Menu::pause();
}

void AppController::reportsMenu() {
    while (true) {
        Menu::header("Reports");
        Menu::options({"Bank-wide overview",
                       "Daily activity",
                       "Loan book",
                       "Top accounts by balance",
                       "Account statement",
                       "Customer summary",
                       "Back"});
        const int c = Menu::choice(1, 7);
        Menu::clear();
        switch (c) {
            case 1: reports_.printBankOverview(std::cout); break;
            case 2: reports_.printDailyActivity(std::cout); break;
            case 3: reports_.printLoanBook(std::cout); break;
            case 4: reports_.printTopAccounts(10, std::cout); break;
            case 5: {
                const std::string number =
                    Validator::readText("  Account number: ");
                reports_.printAccountStatement(number, 50, std::cout);
                break;
            }
            case 6: {
                const int id =
                    Validator::readInt("  Customer ID: ", 1, 100000000);
                reports_.printCustomerSummary(id, std::cout);
                break;
            }
            default: return;
        }
        Menu::pause();
    }
}

}  // namespace bms
