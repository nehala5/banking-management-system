#include "core/Auth.h"

#include "core/Hashing.h"
#include "core/Utils.h"

namespace bms {

Customer* Auth::authenticate(int userId, const std::string& pin) {
    Customer* c = db_.findCustomerById(userId);
    if (!c) return nullptr;
    const std::string salt = std::to_string(c->id);
    if (!Hashing::verifyPin(pin, salt, c->pinHash)) return nullptr;
    return c;
}

Customer* Auth::registerCustomer(const std::string& name,
                                 const std::string& email,
                                 const std::string& phone,
                                 const std::string& address,
                                 const std::string& pin) {
    Customer c;
    c.id = db_.nextCustomerId();
    c.name = name;
    c.email = email;
    c.phone = phone;
    c.address = address;
    c.pinHash = Hashing::hashPin(pin, std::to_string(c.id));
    c.role = Role::CUSTOMER;
    c.createdAt = Utils::now();
    db_.customers().push_back(c);

    // Every new customer gets a savings account automatically.
    Account acc;
    acc.accountNumber = db_.nextAccountNumber();
    acc.customerId = c.id;
    acc.type = AccountType::SAVINGS;
    acc.balancePaise = 0;
    acc.interestRate = 3.5;
    acc.status = AccountStatus::ACTIVE;
    acc.createdAt = Utils::now();
    acc.lastInterestApplied = Utils::today();
    db_.accounts().push_back(acc);

    db_.saveAll();
    return &db_.customers().back();
}

bool Auth::emailExists(const std::string& email) const {
    const std::string needle = Utils::lower(Utils::trim(email));
    for (const auto& c : db_.customers()) {
        if (Utils::lower(c.email) == needle) return true;
    }
    return false;
}

}  // namespace bms
