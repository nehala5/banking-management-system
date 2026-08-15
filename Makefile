# OneSky Bank — Banking Management System
# Build:  make          -> onesky_bank
#         make run      -> build & launch
#         make clean    -> remove build artifacts
# On Windows with mingw32-make:  mingw32-make
# (If you just want to build quickly on Windows, use build.bat instead.)

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude
BIN      := onesky_bank

SRC := src/main.cpp \
       src/models/Customer.cpp \
       src/models/Account.cpp \
       src/models/Transaction.cpp \
       src/models/Loan.cpp \
       src/core/Hashing.cpp \
       src/core/Utils.cpp \
       src/core/Database.cpp \
       src/core/Auth.cpp \
       src/services/AccountService.cpp \
       src/services/TransactionService.cpp \
       src/services/LoanService.cpp \
       src/services/ReportService.cpp \
       src/ui/Validator.cpp \
       src/ui/Menu.cpp \
       src/ui/AppController.cpp

OBJ := $(SRC:.cpp=.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(OBJ) $(BIN) $(BIN).exe

.PHONY: all run clean
