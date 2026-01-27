# Bosch BMP280 Driver
Non-blocking driver for Bosch BMP280 pressure sensor in C language.

# Running Tests
Follow these steps in order to run all unit tests for the driver source code.

Prerequisites:
- CMake
- C compiler (e.g. gcc)
- C++ compiler (e.g. g++)

Steps:
1. Clone this repository
1. Fetch the CppUTest submodule:
    ```
    git submodule init
    git submodule update
    ```
1. Build and run the tests:
    ```
    ./run_tests.sh
    ```
