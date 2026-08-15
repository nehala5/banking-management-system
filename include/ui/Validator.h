#pragma once

#include <string>

namespace bms {

// Console input helpers. Every reader loops until it receives valid input, so
// the rest of the app can assume the values it gets are sane.
class Validator {
public:
    static int readInt(const std::string& prompt, int min, int max);
    static long long readPaise(const std::string& prompt, long long min);
    static double readDouble(const std::string& prompt, double min, double max);

    // Non-empty text with no reserved delimiter character '|'.
    static std::string readName(const std::string& prompt);

    // Anything, but stripped of the delimiter character.
    static std::string readText(const std::string& prompt);

    // Email that contains '@' and '.'.
    static std::string readEmail(const std::string& prompt);

    // 10-digit phone number.
    static std::string readPhone(const std::string& prompt);

    // Numeric PIN of exactly 4 digits.
    static std::string readPin(const std::string& prompt);

    static bool confirm(const std::string& prompt);
};

}  // namespace bms
