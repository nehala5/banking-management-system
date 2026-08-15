#pragma once

#include <string>

#include "core/Database.h"

namespace bms {

// Authentication: PIN verification, customer registration and an active
// session describing who is logged in and in what role.
class Auth {
public:
    explicit Auth(Database& db) : db_(db) {}

    // Verify a customer id + PIN. Returns the matched customer or nullptr.
    Customer* authenticate(int userId, const std::string& pin);

    // Register a brand-new customer. Returns the created customer (with its
    // auto-opened savings account) or nullptr if validation fails.
    Customer* registerCustomer(const std::string& name,
                               const std::string& email,
                               const std::string& phone,
                               const std::string& address,
                               const std::string& pin);

    // Check whether an email is already taken.
    bool emailExists(const std::string& email) const;

private:
    Database& db_;
};

}  // namespace bms
