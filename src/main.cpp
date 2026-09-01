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
            std::cerr << "Error: another tei instance is already running, "
                         "close it first to prevent data corruption\n";
            resetBoldColorIfTerm(stderr);
            return 1;
        }

        return dispatchCli(argc, argv);
    } catch (const std::system_error& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal system error occurred\n";
        std::cerr << "Error message: " << e.what() << '\n';
        std::cerr << "Error code: " << e.code().value() << "\n";
        std::cerr << "Error category: " << e.code().category().name() << "\n";
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (const std::runtime_error& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal runtime error occurred: " << e.what() << '\n';
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (const std::logic_error& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal logic error occurred: " << e.what() << '\n';
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (const std::exception& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal standard exception occurred: " << e.what() << '\n';
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (...) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "Fatal non-standard exception occurred\n";
        resetBoldColorIfTerm(stderr);
        return 1;
    }
}
