#pragma once

#include <string>

namespace bms {

// PIN hashing for the demo. Uses FNV-1a 64-bit over "salt:pin". NOT
// cryptographically secure — in a production system use a real KDF (bcrypt /
// PBKDF2). The point here is that plaintext PINs are never written to disk.
class Hashing {
public:
    static std::string hashPin(const std::string& pin, const std::string& salt);
    static bool verifyPin(const std::string& pin, const std::string& salt,
                          const std::string& expectedHash);
};

}  // namespace bms
