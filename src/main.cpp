#include <exception>
#include <iostream>
#include "test.hpp"

int main() {
    try {
    }
    catch (const std::exception& e) {
        std::cerr << "fatal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
