#include "core/Database.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "core/Hashing.h"
#include "core/Utils.h"

namespace bms {

namespace {
const char* kCustomersFile = "customers.dat";
const char* kAccountsFile = "accounts.dat";
const char* kTransactionsFile = "transactions.dat";
const char* kLoansFile = "loans.dat";
const char* kSeqFile = "seq.dat";
}  // namespace

bool Database::load(const std::string& dataDir) {
    dataDir_ = dataDir;
    if (!ensureDataDir()) return false;

    const bool isFirstRun =
        !std::ifstream(dataDir_ + "/" + kCustomersFile).good();

    // Sequences.
    {
        std::ifstream in(dataDir_ + "/" + kSeqFile);
        if (in) {
            std::string line;
            int n = 0;
            while (std::getline(in, line)) {
                std::istringstream ss(line);
                if (n == 0) ss >> customerSeq_;
                else if (n == 1) ss >> txSeq_;
                else if (n == 2) ss >> accountSeq_;
                else if (n == 3) ss >> loanSeq_;
                ++n;
            }
        }
    }

    loadFile(kCustomersFile, customers_, &Customer::deserialize);
    loadFile(kAccountsFile, accounts_, &Account::deserialize);
    loadFile(kTransactionsFile, transactions_, &Transaction::deserialize);
    loadFile(kLoansFile, loans_, &Loan::deserialize);

    if (isFirstRun) {
        if (!seedDemoData()) return false;
        saveAll();
    }
    return true;
}

bool Database::saveAll() const {
    if (!ensureDataDir()) return false;

    saveFile(kCustomersFile, customers_, &Customer::serialize);
    saveFile(kAccountsFile, accounts_, &Account::serialize);
    saveFile(kTransactionsFile, transactions_, &Transaction::serialize);
    saveFile(kLoansFile, loans_, &Loan::serialize);

    std::ofstream out(dataDir_ + "/" + kSeqFile, std::ios::trunc);
    if (!out) return false;
    out << customerSeq_ << "\n"
        << txSeq_ << "\n"
        << accountSeq_ << "\n"
        << loanSeq_ << "\n";
    return static_cast<bool>(out);
}

std::string Database::nextAccountNumber() {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "OB%06d", ++accountSeq_);
    return std::string(buf);
}

std::string Database::nextLoanId() {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "LN%04d", ++loanSeq_);
    return std::string(buf);
}

Customer* Database::findCustomerById(int id) {
    for (auto& c : customers_)
        if (c.id == id) return &c;
    return nullptr;
}

const Customer* Database::findCustomerById(int id) const {
    for (const auto& c : customers_)
        if (c.id == id) return &c;
    return nullptr;
}

Account* Database::findAccountByNumber(const std::string& number) {
    for (auto& a : accounts_)
        if (a.accountNumber == number) return &a;
    return nullptr;
}

const Account* Database::findAccountByNumber(const std::string& number) const {
    for (const auto& a : accounts_)
        if (a.accountNumber == number) return &a;
    return nullptr;
}

Loan* Database::findLoanById(const std::string& loanId) {
    for (auto& l : loans_)
        if (l.loanId == loanId) return &l;
    return nullptr;
}

void Database::addTransaction(const Transaction& tx) {
    transactions_.push_back(tx);
}

bool Database::ensureDataDir() const {
#ifdef _WIN32
    if (_mkdir(dataDir_.c_str()) == 0) return true;
    return errno == EEXIST;
#else
    if (mkdir(dataDir_.c_str(), 0755) == 0) return true;
    return errno == EEXIST;
#endif
}

bool Database::seedDemoData() {
    // Admin — user id 1000, PIN 1234.
    Customer admin;
    admin.id = ++customerSeq_;
    admin.name = "System Administrator";
    admin.email = "admin@oneskybank.in";
    admin.phone = "0000000000";
    admin.address = "Head Office";
    admin.pinHash = Hashing::hashPin("1234", std::to_string(admin.id));
    admin.role = Role::ADMIN;
    admin.createdAt = Utils::now();
    customers_.push_back(admin);

    // Demo customer — user id 1001, PIN 0000.
    Customer demo;
    demo.id = ++customerSeq_;
    demo.name = "Aarav Sharma";
    demo.email = "aarav@example.com";
    demo.phone = "9812345678";
    demo.address = "Andheri West, Mumbai";
    demo.pinHash = Hashing::hashPin("0000", std::to_string(demo.id));
    demo.role = Role::CUSTOMER;
    demo.createdAt = Utils::now();
    customers_.push_back(demo);

    // Demo savings account.
    Account acc;
    acc.accountNumber = nextAccountNumber();
    acc.customerId = demo.id;
    acc.type = AccountType::SAVINGS;
    acc.balancePaise = 12456020;
    acc.interestRate = 3.5;
    acc.status = AccountStatus::ACTIVE;
    acc.createdAt = Utils::now();
    acc.lastInterestApplied = Utils::today();
    accounts_.push_back(acc);

    // A little transaction history to make reports feel real.
    auto push = [&](TxType t, long long amt, long long bal,
                    const std::string& other, const std::string& remark) {
        Transaction tx;
        tx.id = ++txSeq_;
        tx.accountNumber = acc.accountNumber;
        tx.type = t;
        tx.amountPaise = amt;
        tx.balanceAfterPaise = bal;
        tx.counterparty = other;
        tx.timestamp = Utils::now();
        tx.remark = remark;
        transactions_.push_back(tx);
    };
    push(TxType::DEPOSIT, 8500000, 9760020, "TCS Infotech", "Salary credit");
    push(TxType::WITHDRAWAL, 3500, 9756520, "ATM Andheri", "Cash withdrawal");
    push(TxType::TRANSFER_OUT, 16500, 9740020, "Third Wave Coffee", "UPI payment");
    push(TxType::DEPOSIT, 100000, 9840020, "Self deposit", "Savings deposit");
    push(TxType::INTEREST, 2716, 9842736, "Bank", "Monthly interest @3.5% p.a.");
    acc.balancePaise = 9842736;
    return true;
}

template <typename T>
bool Database::saveFile(const std::string& name,
                        const std::vector<T>& rows,
                        std::string (*serialize)(const T&)) const {
    std::ofstream out(dataDir_ + "/" + name, std::ios::trunc);
    if (!out) return false;
    for (const auto& r : rows) {
        out << serialize(r) << "\n";
    }
    return static_cast<bool>(out);
}

template <typename T>
bool Database::loadFile(const std::string& name,
                        std::vector<T>& rows,
                        bool (*deserialize)(const std::string&, T&)) const {
    std::ifstream in(dataDir_ + "/" + name);
    if (!in) return true;  // missing file == empty collection
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        T row;
        if (deserialize(line, row)) rows.push_back(row);
    }
    return true;
}

}  // namespace bms
