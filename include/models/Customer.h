#pragma once

#include <string>

namespace bms {

enum class Role { CUSTOMER = 0, ADMIN = 1 };

// A customer (or admin) of the bank. Stored in data/customers.dat.
struct Customer {
    int id = 0;
    std::string name;
    std::string email;
    std::string phone;
    std::string address;
    std::string pinHash;
    Role role = Role::CUSTOMER;
    std::string createdAt;   // YYYY-MM-DD HH:MM:SS

    std::string roleName() const;

    // Serialization: one line of '|' separated fields.
    static std::string serialize(const Customer& c);
    static bool deserialize(const std::string& line, Customer& out);
};

}  // namespace bms
