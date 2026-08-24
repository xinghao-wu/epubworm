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
        test::findNth();
        test::execute();
        test::styleEachLineIndividually();
        test::initConf();
        test::initLibrary();
        test::readTeiConf();
        test::getTruncatedSHA256Sum();
        test::findEpubById();
        test::addToLibrary();
        test::queryEpubElem();
        test::writeProgress();
        test::deleteFromLibrary();
        test::getLastRead();
        boldColorIfTerm(stdout, greenFG);
        std::cout << "all self-verifying tests passed\n";
        resetBoldColorIfTerm(stdout);

        return 0;
    } catch (const std::system_error& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "fatal system error occurred\n";
        std::cerr << "error message: " << e.what() << '\n';
        std::cerr << "error code: " << e.code().value() << "\n";
        std::cerr << "error category: " << e.code().category().name() << "\n";
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (const std::runtime_error& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "fatal runtime error occurred: " << e.what() << '\n';
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (const std::logic_error& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "fatal logic error occurred: " << e.what() << '\n';
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (const std::exception& e) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "fatal standard exception occurred: " << e.what() << '\n';
        resetBoldColorIfTerm(stderr);
        return 1;
    } catch (...) {
        boldColorIfTerm(stderr, redFG);
        std::cerr << "fatal non-standard exception occurred\n";
        resetBoldColorIfTerm(stderr);
        return 1;
    }
}
