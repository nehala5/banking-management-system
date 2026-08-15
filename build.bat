@echo off
rem OneSky Bank build script (Windows, MinGW g++).
setlocal
cd /d "%~dp0"
if not exist build mkdir build

g++ -std=c++17 -Wall -Wextra -static -Iinclude ^
  src\main.cpp ^
  src\models\Customer.cpp ^
  src\models\Account.cpp ^
  src\models\Transaction.cpp ^
  src\models\Loan.cpp ^
  src\core\Hashing.cpp ^
  src\core\Utils.cpp ^
  src\core\Database.cpp ^
  src\core\Auth.cpp ^
  src\services\AccountService.cpp ^
  src\services\TransactionService.cpp ^
  src\services\LoanService.cpp ^
  src\services\ReportService.cpp ^
  src\ui\Validator.cpp ^
  src\ui\Menu.cpp ^
  src\ui\AppController.cpp ^
  -o build\onesky_bank.exe

if %errorlevel% neq 0 (
  echo.
  echo Build FAILED.
  exit /b 1
)
echo.
echo Build OK: build\onesky_bank.exe
echo Run it with:  build\onesky_bank
endlocal
