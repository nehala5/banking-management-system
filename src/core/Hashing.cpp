#include "core/Hashing.h"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace bms {

namespace {
uint64_t fnv1a(const std::string& data) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}
}  // namespace

std::string Hashing::hashPin(const std::string& pin, const std::string& salt) {
    uint64_t h = fnv1a(salt + ":" + pin);
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << h;
    return os.str();
}

bool Hashing::verifyPin(const std::string& pin, const std::string& salt,
                        const std::string& expectedHash) {
    return hashPin(pin, salt) == expectedHash;
}

}  // namespace bms
