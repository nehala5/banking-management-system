#pragma once

#include <string>

#include "core/Auth.h"
#include "core/Database.h"
#include "models/Account.h"
#include "models/Customer.h"
#include "services/AccountService.h"
#include "services/LoanService.h"
#include "services/ReportService.h"
#include "services/TransactionService.h"

namespace bms {

// Top-level controller that owns the database and wires the menu flows
// together. All state is kept here; individual menus are plain functions.
class AppController {
public:
    AppController();

    void run();

private:
    void welcome();
    void authMenu();
    void adminMenu(Customer& admin);
    void customerMenu(Customer& cust);

    // Customer flows.
    Account* pickOwnAccount(const Customer& cust);
    void depositFlow(Customer& cust);
    void withdrawFlow(Customer& cust);
    void transferFlow(Customer& cust);
    void historyFlow(Customer& cust);
    void customerLoansMenu(Customer& cust);
    void downloadStatementFlow(Customer& cust);

    // Admin flows.
    void searchCustomersFlow();
    void listCustomersFlow();
    void openAccountFlow();
    void closeAccountFlow();
    void setRateFlow();
    void loanApprovalsFlow();
    void reportsMenu();

    void printAccountCard(const Account& acc) const;

    Database db_;
    Auth auth_;
    AccountService accountSvc_;
    TransactionService txSvc_;
    LoanService loanSvc_;
    ReportService reports_;
};

}  // namespace bms
