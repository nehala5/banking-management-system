#include "core/Utils.h"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <vector>

namespace bms {

namespace {

std::tm localTime(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

std::string dateTimeString(const char* fmt, std::time_t t) {
    std::tm tm = localTime(t);
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt,
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

std::string groupRupees(long long rupees) {
    std::string s = std::to_string(rupees);
    if (s.size() <= 3) return s;
    std::string tail = s.substr(s.size() - 3);
    std::string head = s.substr(0, s.size() - 3);
    std::string out;
    int count = 0;
    for (int i = static_cast<int>(head.size()) - 1; i >= 0; --i) {
        out.insert(out.begin(), head[i]);
        ++count;
        if (count == 2 && i > 0) {
            out.insert(out.begin(), ',');
            count = 0;
        }
    }
    return out + "," + tail;
}

}  // namespace

std::string Utils::now() {
    return dateTimeString("%04d-%02d-%02d %02d:%02d:%02d", std::time(nullptr));
}

std::string Utils::today() {
    return dateTimeString("%04d-%02d-%02d", std::time(nullptr));
}

std::string Utils::addMonths(const std::string& date, int months) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return date;

    std::time_t t = std::time(nullptr);
    std::tm tm = localTime(t);
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1 + months;
    tm.tm_mday = d;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    std::mktime(&tm);  // normalises day overflow

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return std::string(buf);
}

std::string Utils::formatPaise(long long paise) {
    std::string body = formatPaisePlain(paise);
    if (!body.empty() && body[0] == '-') {
        return "-₹" + body.substr(1);
    }
    return "₹" + body;
}

std::string Utils::formatPaisePlain(long long paise) {
    bool neg = paise < 0;
    if (neg) paise = -paise;
    long long rupees = paise / 100;
    long long p = paise % 100;
    std::string out = groupRupees(rupees);
    out += ".";
    out += (p < 10 ? "0" : "") + std::to_string(p);
    return neg ? "-" + out : out;
}

std::string Utils::trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string Utils::lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool Utils::isNumeric(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

std::string Utils::greeting() {
    std::time_t t = std::time(nullptr);
    std::tm tm = localTime(t);
    if (tm.tm_hour < 12) return "Good morning";
    if (tm.tm_hour < 17) return "Good afternoon";
    return "Good evening";
}

}  // namespace bms
