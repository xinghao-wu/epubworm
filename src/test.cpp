#include <filesystem>
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <string>
#include "tinyxml2.hpp"
#include "data_management.hpp"
#include "epub_parser.hpp"
#include "tui.hpp"
#include "test.hpp"

namespace fs = std::filesystem;
using namespace tinyxml2;

namespace test {
    // assumes executable is ran with the working directory being build/
    const fs::path projectRootAbs {fs::current_path().parent_path()};

    const fs::path epubsAbs {projectRootAbs / "test_epubs"};
    const fs::path mysteriesRootAbs {
            epubsAbs / "lord_of_mysteries_vol_1_unzipped"};
    const fs::path parasiteRootAbs {epubsAbs / "parasite_in_love_unzipped"};
    const fs::path spiceWolfRootAbs {
            epubsAbs / "spice_and_wolf_vol_1_unzipped"};
    const fs::path zuttomoRootAbs {epubsAbs / "zuttomo_vol_1_unzipped"};

    const fs::path xdgDirsAbs {projectRootAbs / ".testing_xdg_dirs"};
    const fs::path cacheAbs {xdgDirsAbs / ".cache"};
    const fs::path configAbs {xdgDirsAbs / ".config"};
    const fs::path shareAbs {xdgDirsAbs / "share"};

    void unzip() {
        const fs::path mysteriesZippedAbs {
                epubsAbs / "lord_of_mysteries_vol_1.epub"};
        const fs::path parasiteZippedAbs {epubsAbs / "parasite_in_love.epub"};
        const fs::path spiceWolfZippedAbs {
                epubsAbs / "spice_and_wolf_vol_1.epub"};
        const fs::path zuttomoZippedAbs {epubsAbs / "zuttomo_vol_1.epub"};

        try {
            ::unzip("/bad archive path", shareAbs);
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} == "bad zip");
        }
        try {
            ::unzip(parasiteZippedAbs, "/bad destination path");
            assert(false);
        }
        catch (const fs::filesystem_error& e) {
            assert(std::string_view{e.what()} 
                   == "filesystem error: cannot create directories: "
                      "Permission denied [/bad destination path/META-INF]");
        }
        std::cout << "test::unzip() failure cases passed\n";

        ::unzip(mysteriesZippedAbs, 
                shareAbs / "lord_of_mysteries_vol_1_unzipped");
        ::unzip(parasiteZippedAbs, shareAbs / "parasite_in_love_unzipped");
        ::unzip(spiceWolfZippedAbs, 
                shareAbs / "spice_and_wolf_vol_1_unzipped");
        ::unzip(zuttomoZippedAbs, shareAbs / "zuttomo_vol_1_unzipped");
        std::cout << "test::unzip() success cases did not error, "
                     "check testing share dir to verify correct result\n";
    }

    void getOPFRel() {
        try {
            ::getOPFRel("/bad epub root");
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} 
                   == "Error=XML_ERROR_FILE_NOT_FOUND "
                      "ErrorID=3 (0x3) Line number=0: "
                      "filename=/bad epub root/META-INF/container.xml");
        }
        std::cout << "test::getOPFRel() failure cases passed\n";

        assert(::getOPFRel(mysteriesRootAbs) == "content.opf");
        assert(::getOPFRel(parasiteRootAbs) == "OEBPS/content.opf");
        assert(::getOPFRel(spiceWolfRootAbs) == "content.opf");
        assert(::getOPFRel(zuttomoRootAbs) == "content.opf");
        std::cout << "test::getOPFRel() success cases passed\n";
    }

    void getMetadata() {
        XMLDocument mysteriesOPF {};
        mysteriesOPF.LoadFile(
                (mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs)).c_str());
        XMLDocument parasiteOPF {};
        parasiteOPF.LoadFile(
                (parasiteRootAbs / ::getOPFRel(parasiteRootAbs)).c_str());

        const XMLElement* mysteriesMetadata {::getMetadata(mysteriesOPF)};
        const XMLElement* parasiteMetadata {::getMetadata(parasiteOPF)};

        std::cout << "test::getMetadata() no failure cases\n";

        assert(std::string_view{
               mysteriesMetadata->FirstChildElement("dc:language")->GetText()}
               == "en");
        assert(std::string_view{
               parasiteMetadata->FirstChildElement("dc:publisher")->GetText()}
               == "ASCII Media Works");
        std::cout << "test::getMetadata() success cases passed\n";
    }

    void getTitle() {
        XMLDocument mysteriesOPF {};
        mysteriesOPF.LoadFile(
                (mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs)).c_str());
        XMLDocument spiceWolfOPF {};
        spiceWolfOPF.LoadFile(
                (spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs)).c_str());

        const XMLElement* mysteriesMetadata {::getMetadata(mysteriesOPF)};
        const XMLElement* spiceWolfMetadata {::getMetadata(spiceWolfOPF)};

        std::cout << "test::getTitle() no failure cases\n";

        assert(::getTitle(mysteriesMetadata) 
               == "Lord of Mysteries Volume 1: Clown");
        assert(::getTitle(spiceWolfMetadata) 
               == "Spice and Wolf, Vol. 1");
        std::cout << "test::getTitle() success cases passed\n";
    }

    void getAuthor() {
        XMLDocument mysteriesOPF {};
        mysteriesOPF.LoadFile(
                (mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs)).c_str());
        XMLDocument spiceWolfOPF {};
        spiceWolfOPF.LoadFile(
                (spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs)).c_str());

        const XMLElement* mysteriesMetadata {::getMetadata(mysteriesOPF)};
        const XMLElement* spiceWolfMetadata {::getMetadata(spiceWolfOPF)};

        std::cout << "test::getAuthor() no failure cases\n";

        assert(::getAuthor(mysteriesMetadata) 
               == "Cuttlefish That Loves Diving (爱潜水的乌贼)");
        assert(::getAuthor(spiceWolfMetadata) == "Isuna Hasekura");
        std::cout << "test::getAuthor() success cases passed\n";
    }

    void findAndReplaceAll() {
        std::cout << "test::findAndReplaceAll() no failure cases\n";

        std::string hasTarget {"target, target on the wall,"};
        ::findAndReplaceAll(hasTarget, "target", "mirror");
        assert(hasTarget == "mirror, mirror on the wall,");

        std::string noTarget {"am I the fairest of them all?"};
        ::findAndReplaceAll(noTarget, "target", "mirror");
        assert(noTarget == "am I the fairest of them all?");

        std::cout << "test::findAndReplaceAll() success cases passed\n";
    }

    void wrapForTmuxPassthrough() {
        std::cout << "test::wrapForTmuxPassthrough() no failure cases\n";

        std::string notEmpty {"\033]1337;SetProfile=NewProfileName\007"};
        ::wrapForTmuxPassthrough(notEmpty);
        assert(notEmpty == 
               "\033Ptmux;\033\033]1337;SetProfile=NewProfileName\007\033\\");

        std::string empty {};
        ::wrapForTmuxPassthrough(empty);
        assert(empty == "\033Ptmux;\033\\");

        std::cout << "test::wrapForTmuxPassthrough() success cases passed\n";
    }

    void displayImg() {
        std::string output {};

        try {
            ::displayImg("/bad img path", output);
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} == "Unable to open file");
        }
        std::cout << "test::displayImg() failure cases passed\n";

        ::displayImg(mysteriesRootAbs 
                     / "images/Tarot Club - V01B - Justice.jpg", output);
        ::displayImg(parasiteRootAbs / "OEBPS/Images/cover.jpg", output);
        ::displayImg(parasiteRootAbs / "OEBPS/Images/ascii.png", output);
        ::displayImg(spiceWolfRootAbs / "OEBPS/images/Art_P6.jpg", output);
        ::displayImg(zuttomoRootAbs / "images/image1.jpeg", output);
        ::displayImg(zuttomoRootAbs / "images/image3.png", output);
        ::displayImg(zuttomoRootAbs / "images/image3.png", output, 35, 80);
        std::cout << output;

        std::cout << "test::displayImg() success cases did not error, "
                     "check std::cout for expected images\n";
    }

    void getSpine() {
        std::cout << "test::getSpine() no failure cases\n";

        XMLDocument spiceWolfOPF {};
        spiceWolfOPF.LoadFile(
                (spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs)).c_str());
        const std::vector<fs::path> spiceWolfSpine {::getSpine(spiceWolfOPF)};

        assert(spiceWolfSpine.size() == 43);
        assert(spiceWolfSpine[0] == "toc.ncx");
        assert(spiceWolfSpine[1] == "titlepage.xhtml");

        XMLDocument parasiteOPF {};
        parasiteOPF.LoadFile(
                (parasiteRootAbs / ::getOPFRel(parasiteRootAbs)).c_str());
        const std::vector<fs::path> parasiteSpine {::getSpine(parasiteOPF)};

        assert(parasiteSpine.size() == 15);
        assert(parasiteSpine[0] == "toc.ncx");
        assert(parasiteSpine[1] == "Text/cover.xhtml");
        assert(parasiteSpine[2] == "Text/TitlePage.xhtml");
        assert(parasiteSpine[3] == "Text/insert.xhtml");

        std::cout << "test::getSpine() success cases passed\n";
    }

    void getTOC() {
        std::cout << "test::getTOC() no failure cases\n";

        const TocData mysteriesTOC {::getTOC(mysteriesRootAbs / "toc.ncx")};
        assert(mysteriesTOC.size() == 227);
        assert(mysteriesTOC[0].first == "Front Cover");
        assert(mysteriesTOC[0].second == "titlepage.xhtml");
        assert(mysteriesTOC[222].first == "    Characters");
        assert(mysteriesTOC[222].second == "index_split_225.html");

        const TocData parasiteTOC {
                ::getTOC(parasiteRootAbs / "OEBPS/toc.ncx")};
        assert(parasiteTOC.size() == 13);
        assert(parasiteTOC[0].first == "Cover");
        assert(parasiteTOC[0].second == "Text/cover.xhtml");
        assert(parasiteTOC[2].first == "Prologue");
        assert(parasiteTOC[2].second == "Text/insert.xhtml");

        std::cout << "test::getTOC() success cases passed\n";
    }

    void parseChapter() {
        std::cout << "test::parseChapter() no failure cases\n";

        std::string result {};
        ::parseChapter(spiceWolfRootAbs / "OEBPS/chap01.xhtml", result);
        ::parseChapter(spiceWolfRootAbs / "OEBPS/chap02.xhtml", result);
        ::parseChapter(spiceWolfRootAbs / "OEBPS/chapter005.xhtml", result);
        std::cout << result;

        std::cout << "test::parseChapter() success cases did not error, "
                     "check std::cout for correct images and text\n";
    }

    void dumpEpub() {
        std::cout << "test::dumpEpub() no failure cases\n";

        useSystemLocale();

        std::string result {};
        ::dumpEpub(parasiteRootAbs, result);
        ::dumpEpub(spiceWolfRootAbs, result);
        ::dumpEpub(zuttomoRootAbs, result);

        ::expandEllipsesAndTabs(result);
        constexpr int cols {55};
        ::wrapLines(result, cols);
        ::centerJustify(esc + yellowFG, esc + resetFG, result, cols);
        ::centerJustify(esc + redFG, esc + resetFG, result, cols);
        ::centerOnScreenAndAddEraseLineSeq(result, cols);
        std::cout << result;

        std::cout << "test::dumpEpub() success cases did not error, "
                     "check std::cout for content of three epubs. "
                     "Ellipses should be expanded, lines wrapped to 55 cols, "
                     "chapter titles and ends center justified, "
                     "and content centered on the screen.\n";
    }

    void rawReadKey() {
        std::cout << "test::rawReadKey() no failure cases\n";

        std::cout << "test::rawReadKey() success cases needs manual check, "
                     "the terminal should now be in raw mode. "
                     "Any character typed should be instantly displayed "
                     "along with their integer representation. "
                     "Also check for values contained in key::keyValues. "
                     "Press q to quit.\n";

        ::enableRawMode();

        int ch {};
        while ((ch = ::rawReadKey()) != 'q') {
            if (ch >= key::arrowLeft || std::iscntrl(ch) != 0) {
                std::cout << "ctrl char : [" << ch << "]\n" << std::flush;
            }
            else {
                std::cout << '\"' << static_cast<char>(ch) << '\"';
                std::cout << " : [" << ch << "]\n" << std::flush;
            }
        }
    }

    void utf8ToWide() {
        std::cout << "test::utf8ToWide() no failure cases\n";

        assert(::utf8ToWide("hallo") == L"hallo");
        assert(::utf8ToWide("Hello, 世界") == L"Hello, 世界");
        assert(::utf8ToWide("Hello, World! 🚀") == L"Hello, World! 🚀");

        std::cout << "test::utf8ToWide() success cases passed\n";
    }

    void wideToUTF8() {
        std::cout << "test::wideToUTF8() no failure cases\n";

        assert(::wideToUTF8(L"hallo") == "hallo");
        assert(::wideToUTF8(L"Hello, 世界") == "Hello, 世界");
        assert(::wideToUTF8(L"Hello, World! 🚀") == "Hello, World! 🚀");

        std::cout << "test::wideToUTF8() success cases passed\n";
    }

    void findNth() {
        std::cout << "test::findNth() no failure cases\n";

        static_assert(::findNth("banana", "an", 2) == 3);
        static_assert(::findNth("mirra mirra on ze walle", "mirra", 1, 3) 
                      == 6);
        static_assert(::findNth("mirra mirra", "mirra", 3) 
                      == std::string_view::npos);
        std::cout << "test::findNth() success cases passed\n";
    }
}
