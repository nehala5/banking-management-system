#include "ui/Validator.h"

#include <cctype>
#include <iostream>
#include <sstream>

#include "core/Utils.h"

namespace bms {

namespace {

std::string readLine() {
    std::string line;
    std::getline(std::cin, line);
    return Utils::trim(line);
}

void prompt(const std::string& p) {
    std::cout << p;
}

}  // namespace

int Validator::readInt(const std::string& p, int min, int max) {
    while (true) {
        prompt(p);
        const std::string line = readLine();
        if (line.empty() || !Utils::isNumeric(line)) {
            std::cout << "  Please enter a valid number.\n";
            continue;
        }
        int value = 0;
        try {
            value = std::stoi(line);
        } catch (...) {
            std::cout << "  Number out of range.\n";
            continue;
        }
        if (value < min || value > max) {
            std::cout << "  Value must be between " << min << " and " << max
                      << ".\n";
            continue;
        }
        return value;
    }
}

long long Validator::readPaise(const std::string& p, long long min) {
    while (true) {
        prompt(p);
        const std::string line = readLine();
        if (line.empty()) {
            std::cout << "  Please enter an amount.\n";
            continue;
        }

        long long rupees = 0;
        int paise = 0;
        bool ok = true;
        const size_t dot = line.find('.');
        const std::string whole = dot == std::string::npos
                                      ? line
                                      : line.substr(0, dot);
        const std::string frac = dot == std::string::npos
                                     ? ""
                                     : line.substr(dot + 1);

        if (whole.empty() || !Utils::isNumeric(whole)) {
            ok = false;
        } else {
            try {
                rupees = std::stoll(whole);
            } catch (...) {
                ok = false;
            }
        }
        if (ok && frac.size() > 2) {
            std::cout << "  Use at most two decimal places.\n";
            continue;
        }
        if (ok && !frac.empty()) {
            std::string f = frac;
            while (f.size() < 2) f.push_back('0');
            if (!Utils::isNumeric(f)) {
                ok = false;
            } else {
                paise = std::stoi(f);
            }
        }
        if (!ok) {
            std::cout << "  Invalid amount. Use format like 1500 or 1500.50\n";
            continue;
        }

        const long long total = rupees * 100 + paise;
        if (total < min) {
            std::cout << "  Amount must be at least "
                      << Utils::formatPaise(min) << ".\n";
            continue;
        }
        return total;
    }
}

double Validator::readDouble(const std::string& p, double min, double max) {
    while (true) {
        prompt(p);
        const std::string line = readLine();
        if (line.empty()) {
            std::cout << "  Please enter a number.\n";
            continue;
        }
        double value = 0.0;
        try {
            value = std::stod(line);
        } catch (...) {
            std::cout << "  Invalid number.\n";
            continue;
        }
        if (value < min || value > max) {
            std::cout << "  Value must be between " << min << " and " << max
                      << ".\n";
            continue;
        }
        return value;
    }
}

std::string Validator::readName(const std::string& p) {
    while (true) {
        prompt(p);
        std::string line = readLine();
        if (line.empty()) {
            std::cout << "  This field cannot be empty.\n";
            continue;
        }
        std::string cleaned;
        for (char c : line) {
            if (c != '|') cleaned.push_back(c);
        }
        return cleaned;
    }
}

std::string Validator::readText(const std::string& p) {
    prompt(p);
    std::string line = readLine();
    std::string cleaned;
    for (char c : line) {
        if (c != '|') cleaned.push_back(c);
    }
    return cleaned;
}

std::string Validator::readEmail(const std::string& p) {
    while (true) {
        prompt(p);
        const std::string line = readLine();
        if (line.empty() || line.find('@') == std::string::npos ||
            line.find('.') == std::string::npos) {
            std::cout << "  Please enter a valid email address.\n";
            continue;
        }
        return line;
    }
}

std::string Validator::readPhone(const std::string& p) {
    while (true) {
        prompt(p);
        const std::string line = readLine();
        if (line.size() != 10 || !Utils::isNumeric(line)) {
            std::cout << "  Please enter a 10-digit phone number.\n";
            continue;
        }
        return line;
    }
}

std::string Validator::readPin(const std::string& p) {
    while (true) {
        prompt(p);
        const std::string line = readLine();
        if (line.size() != 4 || !Utils::isNumeric(line)) {
            std::cout << "  PIN must be exactly 4 digits.\n";
            continue;
        }
        return line;
    }
}

bool Validator::confirm(const std::string& p) {
    while (true) {
        prompt(p + " [y/N]: ");
        const std::string line = Utils::lower(readLine());
        if (line == "y" || line == "yes") return true;
        if (line == "n" || line == "no" || line.empty()) return false;
        std::cout << "  Please answer y or n.\n";
    }
}

}  // namespace bms
