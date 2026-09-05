#include "test.hpp"
#include "tui.hpp"
#include <exception>
#include <iostream>
#include <stdexcept>
#include <system_error>

int main() {
    try {
        test::getOPFRel();
        test::getMetadata();
        test::getTitle();
        test::getAuthor();
        test::findAndReplaceAll();
        test::wrapForTmuxPassthrough();
        test::getSpine();
        test::getTOC();
        test::utf8ToWide();
        test::wideToUTF8();
        test::collapseConsecutiveNewlines();
        test::findNth();
        test::execute();
        test::tocDataToString();
        test::styleEachLineIndividually();
        test::initConf();
        test::initLibrary();
        test::readConfig();
        test::getTruncatedSHA256Sum();
        test::findEpubById();
        test::getUnambiguousEpubIdPrefix();
        test::addToLibrary();
        test::queryEpubElem();
        test::writeProgress();
        test::deleteFromLibrary();
        test::getLastRead();
        test::singleInstanceLock();
        boldColorIfTerm(stdout, greenFG);
        std::cout << "all self-verifying tests passed\n";
        resetBoldColorIfTerm(stdout);

        return 0;
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
