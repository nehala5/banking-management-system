#pragma once

#include <string>

namespace bms {

class Utils {
public:
    // Current local date/time as "YYYY-MM-DD HH:MM:SS".
    static std::string now();

    // Current local date as "YYYY-MM-DD".
    static std::string today();

    // Add months to a "YYYY-MM-DD" date, clamped to valid calendar days.
    static std::string addMonths(const std::string& date, int months);

    // Format paise with Indian digit grouping, e.g. 12456020 -> ₹1,24,560.20.
    static std::string formatPaise(long long paise);

    // Format paise without the rupee symbol, e.g. 12456020 -> 1,24,560.20.
    static std::string formatPaisePlain(long long paise);

    static std::string trim(const std::string& s);
    static std::string lower(const std::string& s);
    static bool isNumeric(const std::string& s);

    // Short greeting based on current hour (e.g. "Good morning").
    static std::string greeting();
};

}  // namespace bms
