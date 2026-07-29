#include <exception>
#include <stdexcept>
#include <system_error>
#include <iostream>

int main() {
    try {
        return 0;
    }
    catch (const std::system_error& e) {
        std::cerr << "fatal system error occurred\n";
        std::cerr << "error message: " << e.what() << '\n';
        std::cerr << "error code: " << e.code().value() << "\n";
        std::cerr << "error category: " << e.code().category().name() << "\n";
        return 1;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "fatal runtime error occurred: " << e.what() << '\n';
        return 1;
    }
    catch (const std::logic_error& e) {
        std::cerr << "fatal logic error occurred: " << e.what() << '\n';
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "fatal standard exception occurred: " << e.what() << '\n';
        return 1;
    }
    catch (...) {
        std::cerr << "fatal non-standard exception occurred\n";
        return 1;
    }
}
