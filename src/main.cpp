#include <cstdlib>
#include <ctime>

#include "ui/AppController.h"

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    bms::AppController app;
    app.run();
    return 0;
}
