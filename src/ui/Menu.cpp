#include "ui/Menu.h"

#include <cstdlib>
#include <iostream>

#include "core/Utils.h"

namespace bms {

void Menu::clear() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

void Menu::header(const std::string& title) {
    clear();
    const int width = 62;
    std::cout << std::string(width, '=') << "\n";
    std::cout << "  ONESKY BANK — BANKING MANAGEMENT SYSTEM\n";
    std::cout << std::string(width, '=') << "\n";
    if (!title.empty()) {
        std::cout << "  " << title << "\n";
        std::cout << std::string(width, '-') << "\n";
    }
}

void Menu::footer() {
    std::cout << std::string(62, '=') << "\n";
}

void Menu::options(const std::vector<std::string>& items) {
    int i = 1;
    for (const auto& item : items) {
        std::cout << "  [" << i++ << "] " << item << "\n";
    }
    std::cout << std::string(62, '-') << "\n";
}

int Menu::choice(int min, int max) {
    while (true) {
        std::cout << "  Choose: ";
        std::string line;
        std::getline(std::cin, line);
        line = Utils::trim(line);
        if (line.empty() || !Utils::isNumeric(line)) {
            std::cout << "  Invalid choice.\n";
            continue;
        }
        int v = 0;
        try {
            v = std::stoi(line);
        } catch (...) {
            std::cout << "  Invalid choice.\n";
            continue;
        }
        if (v < min || v > max) {
            std::cout << "  Invalid choice.\n";
            continue;
        }
        return v;
    }
}

void Menu::pause() {
    std::cout << "\n  Press Enter to continue...";
    std::cin.ignore();
}

void Menu::showSuccess(const std::string& message) {
    std::cout << "\n  [OK] " << message << "\n";
}

void Menu::showError(const std::string& message) {
    std::cout << "\n  [!] " << message << "\n";
}

}  // namespace bms
