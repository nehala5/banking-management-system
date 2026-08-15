#include "models/Customer.h"

#include <sstream>
#include <vector>

namespace bms {

std::string Customer::roleName() const {
    return role == Role::ADMIN ? "ADMIN" : "CUSTOMER";
}

std::string Customer::serialize(const Customer& c) {
    std::ostringstream os;
    os << c.id << '|'
       << c.name << '|'
       << c.email << '|'
       << c.phone << '|'
       << c.address << '|'
       << c.pinHash << '|'
       << static_cast<int>(c.role) << '|'
       << c.createdAt;
    return os.str();
}

bool Customer::deserialize(const std::string& line, Customer& out) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == '|') {
            f.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    f.push_back(cur);

    if (f.size() != 8) return false;
    try {
        out.id = std::stoi(f[0]);
        out.name = f[1];
        out.email = f[2];
        out.phone = f[3];
        out.address = f[4];
        out.pinHash = f[5];
        out.role = static_cast<Role>(std::stoi(f[6]));
        out.createdAt = f[7];
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace bms
