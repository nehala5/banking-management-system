#pragma once

#include <string>
#include <vector>

namespace bms {

class Menu {
public:
    static void clear();
    static void header(const std::string& title);
    static void footer();
    static void options(const std::vector<std::string>& items);
    static int choice(int min, int max);
    static void pause();
    static void showSuccess(const std::string& message);
    static void showError(const std::string& message);
};

}  // namespace bms
