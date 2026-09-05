#include "cli.hpp"
#include "single_instance.hpp"
#include "tui.hpp"
#include <exception>
#include <iostream>
#include <stdexcept>
#include <system_error>

int main(int argc, char* argv[]) {
    try {
        const SingleInstanceLock instanceLock{getInstanceLockPath()};
        if (!instanceLock.isFirstInstance()) {
            boldColorIfTerm(stderr, redFG);
            std::cerr << "Error: ";
            resetBoldColorIfTerm(stderr);
            std::cerr << "another epubworm instance is already running, "
                         "close it first to prevent data corruption\n";
            return 1;
        }

        return dispatchCli(argc, argv);
    } catch (const std::system_error& e) {
        std::cerr << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal error: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << "system error occurred\n";
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Error message: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << e.what() << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Error code: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << e.code().value() << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Error category: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << e.code().category().name() << '\n';
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal error: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << "runtime error occurred: " << e.what() << '\n';
        return 1;
    } catch (const std::logic_error& e) {
        std::cerr << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal error: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << "logic error occurred: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal error: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << "standard exception occurred: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << '\n';
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal error: ";
        resetBoldColorIfTerm(stderr);
        std::cerr << "non-standard exception occurred\n";
        return 1;
    }
}
