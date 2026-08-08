#include "test.hpp"
#include "data_management.hpp"
#include "epub_parser.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

namespace test {
// Assumes executable is run with the working directory being `build/`.
const fs::path projectRootAbs{fs::current_path().parent_path()};

const fs::path epubsAbs{projectRootAbs / "test_epubs"};
const fs::path mysteriesRootAbs{epubsAbs / "lord_of_mysteries_vol_1_unzipped"};
const fs::path parasiteRootAbs{epubsAbs / "parasite_in_love_unzipped"};
const fs::path spiceWolfRootAbs{epubsAbs / "spice_and_wolf_vol_1_unzipped"};
const fs::path zuttomoRootAbs{epubsAbs / "zuttomo_vol_1_unzipped"};

const fs::path xdgDirsAbs{projectRootAbs / ".testing_xdg_dirs"};
const fs::path dotCacheAbs{xdgDirsAbs / ".cache"};
const fs::path dotConfigAbs{xdgDirsAbs / ".config"};
const fs::path shareAbs{xdgDirsAbs / "share"};

void unzip() {
    const fs::path mysteriesZippedAbs{epubsAbs
                                      / "lord_of_mysteries_vol_1.epub"};
    const fs::path parasiteZippedAbs{epubsAbs / "parasite_in_love.epub"};
    const fs::path spiceWolfZippedAbs{epubsAbs / "spice_and_wolf_vol_1.epub"};
    const fs::path zuttomoZippedAbs{epubsAbs / "zuttomo_vol_1.epub"};

    try {
        ::unzip("/bad archive path", shareAbs);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()} == "bad zip");
    }
    try {
        ::unzip(parasiteZippedAbs, "/bad destination path");
        assert(false);
    } catch (const fs::filesystem_error& e) {
        assert(std::string_view{e.what()}
               == "filesystem error: cannot create directories: "
                  "Permission denied [/bad destination path/META-INF]");
    }
    std::cout << "`test::unzip()` failure cases passed\n";

    ::unzip(mysteriesZippedAbs, shareAbs / "lord_of_mysteries_vol_1_unzipped");
    ::unzip(parasiteZippedAbs, shareAbs / "parasite_in_love_unzipped");
    ::unzip(spiceWolfZippedAbs, shareAbs / "spice_and_wolf_vol_1_unzipped");
    ::unzip(zuttomoZippedAbs, shareAbs / "zuttomo_vol_1_unzipped");
    std::cout << "`test::unzip()` success cases did not error, "
                 "check `.testing_xdg_dirs/share/` to verify correct result\n";
}

void getOPFRel() {
    try {
        (void)::getOPFRel("/bad epub root");
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "Error=XML_ERROR_FILE_NOT_FOUND "
                  "ErrorID=3 (0x3) Line number=0: "
                  "filename=/bad epub root/META-INF/container.xml");
    }
    std::cout << "`test::getOPFRel()` failure cases passed\n";

    assert(::getOPFRel(mysteriesRootAbs) == "content.opf");
    assert(::getOPFRel(parasiteRootAbs) == "OEBPS/content.opf");
    assert(::getOPFRel(spiceWolfRootAbs) == "content.opf");
    assert(::getOPFRel(zuttomoRootAbs) == "content.opf");
    std::cout << "`test::getOPFRel()` success cases passed\n";
}

void getMetadata() {
    XMLDocument mysteriesOPF{};
    mysteriesOPF.LoadFile(
            (mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs)).c_str());
    XMLDocument parasiteOPF{};
    parasiteOPF.LoadFile(
            (parasiteRootAbs / ::getOPFRel(parasiteRootAbs)).c_str());

    const XMLElement* mysteriesMetadata{::getMetadata(mysteriesOPF)};
    const XMLElement* parasiteMetadata{::getMetadata(parasiteOPF)};

    std::cout << "`test::getMetadata()` no failure cases\n";

    assert(std::string_view{mysteriesMetadata->FirstChildElement("dc:language")
                                    ->GetText()}
           == "en");
    assert(std::string_view{parasiteMetadata->FirstChildElement("dc:publisher")
                                    ->GetText()}
           == "ASCII Media Works");
    std::cout << "`test::getMetadata()` success cases passed\n";
}

void getTitle() {
    XMLDocument mysteriesOPF{};
    mysteriesOPF.LoadFile(
            (mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs)).c_str());
    XMLDocument spiceWolfOPF{};
    spiceWolfOPF.LoadFile(
            (spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs)).c_str());

    const XMLElement* mysteriesMetadata{::getMetadata(mysteriesOPF)};
    const XMLElement* spiceWolfMetadata{::getMetadata(spiceWolfOPF)};

    std::cout << "`test::getTitle()` no failure cases\n";

    assert(::getTitle(mysteriesMetadata)
           == "Lord of Mysteries Volume 1: Clown");
    assert(::getTitle(spiceWolfMetadata) == "Spice and Wolf, Vol. 1");
    std::cout << "`test::getTitle()` success cases passed\n";
}

void getAuthor() {
    XMLDocument mysteriesOPF{};
    mysteriesOPF.LoadFile(
            (mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs)).c_str());
    XMLDocument spiceWolfOPF{};
    spiceWolfOPF.LoadFile(
            (spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs)).c_str());

    const XMLElement* mysteriesMetadata{::getMetadata(mysteriesOPF)};
    const XMLElement* spiceWolfMetadata{::getMetadata(spiceWolfOPF)};

    std::cout << "`test::getAuthor()` no failure cases\n";

    assert(::getAuthor(mysteriesMetadata)
           == "Cuttlefish That Loves Diving (爱潜水的乌贼)");
    assert(::getAuthor(spiceWolfMetadata) == "Isuna Hasekura");
    std::cout << "`test::getAuthor()` success cases passed\n";
}

void findAndReplaceAll() {
    std::cout << "`test::findAndReplaceAll()` no failure cases\n";

    std::string hasTarget{"target, target on the wall,"};
    ::findAndReplaceAll(hasTarget, "target", "mirror");
    assert(hasTarget == "mirror, mirror on the wall,");

    std::string noTarget{"am I the fairest of them all?"};
    ::findAndReplaceAll(noTarget, "target", "mirror");
    assert(noTarget == "am I the fairest of them all?");

    std::cout << "`test::findAndReplaceAll()` success cases passed\n";
}

void wrapForTmuxPassthrough() {
    std::cout << "`test::wrapForTmuxPassthrough()` no failure cases\n";

    std::string notEmpty{"\033]1337;SetProfile=NewProfileName\007"};
    ::wrapForTmuxPassthrough(notEmpty);
    assert(notEmpty
           == "\033Ptmux;\033\033]1337;SetProfile=NewProfileName\007\033\\");

    std::string empty{};
    ::wrapForTmuxPassthrough(empty);
    assert(empty == "\033Ptmux;\033\\");

    std::cout << "`test::wrapForTmuxPassthrough()` success cases passed\n";
}

void displayImg() {
    std::string output{};

    try {
        ::displayImg("/bad img path", output);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()} == "Unable to open file");
    }
    std::cout << "`test::displayImg()` failure cases passed\n";

    ::displayImg(mysteriesRootAbs / "images/Tarot Club - V01B - Justice.jpg",
                 output);
    ::displayImg(parasiteRootAbs / "OEBPS/Images/cover.jpg", output);
    ::displayImg(parasiteRootAbs / "OEBPS/Images/ascii.png", output);
    ::displayImg(spiceWolfRootAbs / "OEBPS/images/Art_P6.jpg", output);
    ::displayImg(zuttomoRootAbs / "images/image1.jpeg", output);
    ::displayImg(zuttomoRootAbs / "images/image3.png", output);
    ::displayImg(zuttomoRootAbs / "images/image3.png", output, 35, 80);
    std::cout << output;

    std::cout << "`test::displayImg()` success cases did not error, "
                 "check `std::cout` for expected images\n";
}

void getSpine() {
    std::cout << "`test::getSpine()` no failure cases\n";

    XMLDocument spiceWolfOPF{};
    spiceWolfOPF.LoadFile(
            (spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs)).c_str());
    const std::vector<fs::path> spiceWolfSpine{::getSpine(spiceWolfOPF)};

    assert(spiceWolfSpine.size() == 43);
    assert(spiceWolfSpine[0] == "toc.ncx");
    assert(spiceWolfSpine[1] == "titlepage.xhtml");

    XMLDocument parasiteOPF{};
    parasiteOPF.LoadFile(
            (parasiteRootAbs / ::getOPFRel(parasiteRootAbs)).c_str());
    const std::vector<fs::path> parasiteSpine{::getSpine(parasiteOPF)};

    assert(parasiteSpine.size() == 15);
    assert(parasiteSpine[0] == "toc.ncx");
    assert(parasiteSpine[1] == "Text/cover.xhtml");
    assert(parasiteSpine[2] == "Text/TitlePage.xhtml");
    assert(parasiteSpine[3] == "Text/insert.xhtml");

    std::cout << "`test::getSpine()` success cases passed\n";
}

void getTOC() {
    std::cout << "`test::getTOC()` no failure cases\n";

    const TocData mysteriesTOC{::getTOC(mysteriesRootAbs / "toc.ncx")};
    assert(mysteriesTOC.size() == 227);
    assert(mysteriesTOC[0].first == "Front Cover");
    assert(mysteriesTOC[0].second == "titlepage.xhtml");
    assert(mysteriesTOC[222].first == "    Characters");
    assert(mysteriesTOC[222].second == "index_split_225.html");

    const TocData parasiteTOC{::getTOC(parasiteRootAbs / "OEBPS/toc.ncx")};
    assert(parasiteTOC.size() == 13);
    assert(parasiteTOC[0].first == "Cover");
    assert(parasiteTOC[0].second == "Text/cover.xhtml");
    assert(parasiteTOC[2].first == "Prologue");
    assert(parasiteTOC[2].second == "Text/insert.xhtml");

    std::cout << "`test::getTOC()` success cases passed\n";
}

void parseChapter() {
    std::cout << "`test::parseChapter()` no failure cases\n";

    std::string result{};
    ::parseChapter(spiceWolfRootAbs / "OEBPS/chap01.xhtml", result);
    ::parseChapter(spiceWolfRootAbs / "OEBPS/chap02.xhtml", result);
    ::parseChapter(spiceWolfRootAbs / "OEBPS/chapter005.xhtml", result);
    std::cout << result;

    std::cout << "`test::parseChapter()` success cases did not error, "
                 "check `std::cout` for correct images and text\n";
}

void dumpEpub() {
    std::cout << "`test::dumpEpub()` no failure cases\n";

    useSystemLocale();

    std::string result{};
    ::dumpEpub(parasiteRootAbs, result);
    ::dumpEpub(spiceWolfRootAbs, result);
    ::dumpEpub(zuttomoRootAbs, result);
    ::processContentText(result, 55);
    std::cout << result;

    std::cout << "`test::dumpEpub()` success cases did not error, "
                 "check `std::cout` for content of three epubs. "
                 "Ellipses should be expanded, lines wrapped to 55 cols, "
                 "chapter titles and ends center justified, "
                 "and content centered on the screen.\n";
}

void readRawInput() {
    std::cout << "`test::readRawInput()` no failure cases\n";

    std::cout << "`test::readRawInput()` success cases needs manual check, "
                 "the terminal should now be in raw mode. "
                 "Verify correct key, row, col values for inputs. "
                 "Press q to quit.\n";

    ::enableRawMode();

    std::tuple<Key, int, int> input{::readRawInput()};
    while (std::get<0>(input) != 'q') {
        std::cout << "key: " << std::get<0>(input) << '\n';
        std::cout << "row: " << std::get<1>(input) << '\n';
        std::cout << "col: " << std::get<2>(input) << '\n';
        input = ::readRawInput();
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
    std::cout << "`test::findNth()` no failure cases\n";

    static_assert(::findNth("banana", "an", 2) == 3);
    static_assert(::findNth("mirra mirra on ze walle", "mirra", 1, 3) == 6);
    static_assert(::findNth("mirra mirra", "mirra", 3)
                  == std::string_view::npos);
    std::cout << "`test::findNth()` success cases passed\n";
}

void execute() {
    try {
        ::execute(std::vector<std::string>{});
        assert(false);
    } catch (const std::invalid_argument& e) {
        assert(std::string_view{e.what()} == "execute() cmd cannot be empty");
    }
    try {
        ::execute(std::vector<std::string>{""});
        assert(false);
    } catch (const std::invalid_argument& e) {
        assert(std::string_view{e.what()} == "execute() cmd cannot be empty");
    }
    try {
        ::execute(std::vector<std::string>{"nonexistent_cmd_xyz"});
        assert(false);
    } catch (const std::system_error& e) {
        assert(std::string_view{e.what()}.starts_with(
                "failed to spawn cmd: "));
    }
    try {
        ::execute(std::vector<std::string>{"false"});
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "cmd did not exit properly: false");
    }
    std::cout << "`test::execute()` failure cases passed\n";

    assert(::execute(std::vector<std::string>{"true"}).empty());
    assert(::execute(std::vector<std::string>{"echo", "hello"}) == "hello\n");
    assert(::execute(std::vector<std::string>{"printf", "a\\nb\\nc\\n"})
           == "a\nb\nc\n");

    const std::string largeOutput{
            ::execute(std::vector<std::string>{"seq", "50000"})};
    assert(largeOutput.size() > 65536);
    assert(largeOutput.starts_with("1\n"));
    assert(largeOutput.ends_with("50000\n"));

    std::cout << "`test::execute()` success cases passed\n";
}

void displayChapter() {
    ::useSystemLocale();
    ::enableRawMode();

    std::cout << esc << clearScreen;
    const std::pair imgChapterOutput{
            ::displayChapter(spiceWolfRootAbs / "OEBPS/chap02.xhtml", 0, 55)};
    const std::pair textChapterOutput{::displayChapter(
            spiceWolfRootAbs / "OEBPS/chapter005.xhtml", 0.5, 55)};
    eraseScreen();

    std::cout << "`test::displayChapter()` no failure cases\n";
    std::cout << "`test::displayChapter()` success cases require manual "
                 "verification, a tui interface for an image and text "
                 "chapter should have been displayed.\n";

    std::cout << "image chapter exit reason: ";
    std::cout << static_cast<int>(imgChapterOutput.first);
    std::cout << '\n';
    std::cout << "image chapter final prog: " << imgChapterOutput.second;
    std::cout << '\n';
    std::cout << "text chapter exit reason: ";
    std::cout << static_cast<int>(textChapterOutput.first);
    std::cout << '\n';
    std::cout << "text chapter final prog: " << textChapterOutput.second;
    std::cout << '\n';
}

void tocDataToString() {
    std::cout << "`test::tocDataToString()` no failure cases\n";

    ::useSystemLocale();

    const TocData mysteriesTOC{::getTOC(mysteriesRootAbs / "toc.ncx")};
    const TocData parasiteTOC{::getTOC(parasiteRootAbs / "OEBPS/toc.ncx")};

    std::string str{};
    ::tocDataToString(mysteriesTOC, str);
    ::tocDataToString(parasiteTOC, str);
    ::processContentText(str, 55);
    std::cout << str;

    std::cout << "`test::tocDataToString()` success cases did not error, "
                 "verify correct table of content strings are shown\n";
}

void displayTOC() {
    ::useSystemLocale();
    ::enableRawMode();

    std::cout << esc << clearScreen;
    const fs::path mysteriesTOCOutput{
            ::displayTOC(::getTOC(mysteriesRootAbs / "toc.ncx"), 55, 100)};
    const fs::path parasiteTOCOutput{
            ::displayTOC(::getTOC(parasiteRootAbs / "OEBPS/toc.ncx"), 55, 0)};
    eraseScreen();

    std::cout << "`test::displayTOC()` no failure cases\n";
    std::cout << "`test::displayTOC()` success cases require manual "
                 "verification, a tui interface for mysteries' "
                 "and parasite's TOCs should have been displayed.\n";

    std::cout << "mysteries output: " << mysteriesTOCOutput << '\n';
    std::cout << "parasite output: " << parasiteTOCOutput << '\n';
}

void displayEpub() {
    ::useSystemLocale();
    ::enableRawMode();

    const EpubProg mysteriesIniProg{mysteriesRootAbs / "index_split_117.html",
                                    0.5};
    const EpubProg parasiteIniProg{"", 0};
    const EpubProg spiceWolfIniProg{
            spiceWolfRootAbs / "OEBPS/epilogue-a.xhtml", 0.25};
    const EpubProg zuttomoIniProg{zuttomoRootAbs / "index_split_022.html",
                                  0.75};

    const EpubProg mysteriesOut{
            ::displayEpub(mysteriesIniProg, mysteriesRootAbs, 50)};
    const EpubProg parasiteOut{
            ::displayEpub(parasiteIniProg, parasiteRootAbs, 55)};
    const EpubProg spiceWolfOut{
            ::displayEpub(spiceWolfIniProg, spiceWolfRootAbs, 60)};
    const EpubProg zuttomoOut{
            ::displayEpub(zuttomoIniProg, zuttomoRootAbs, 65)};

    std::cout << "`test::displayEpub()` no failure cases\n";
    std::cout << "`test::displayEpub()` success cases require manual "
                 "verification, a tui interface for all four "
                 "test epubs should have been displayed.\n";

    std::cout << "mysteries exit chapter path: ";
    std::cout << mysteriesOut.chapterAbs << '\n';
    std::cout << "mysteries exit chapter prog: ";
    std::cout << mysteriesOut.chapterProg << '\n';
    std::cout << "parasite exit chapter path: ";
    std::cout << parasiteOut.chapterAbs << '\n';
    std::cout << "parasite exit chapter prog: ";
    std::cout << parasiteOut.chapterProg << '\n';
    std::cout << "spiceWolf exit chapter path: ";
    std::cout << spiceWolfOut.chapterAbs << '\n';
    std::cout << "spiceWolf exit chapter prog: ";
    std::cout << spiceWolfOut.chapterProg << '\n';
    std::cout << "zuttomo exit chapter path: ";
    std::cout << zuttomoOut.chapterAbs << '\n';
    std::cout << "zuttomo exit chapter prog: ";
    std::cout << zuttomoOut.chapterProg << '\n';
}

void styleEachLineIndividually() {
    std::cout << "`test::styleEachLineIndividually()` no failure cases\n";

    std::string str{"\033[1mfirst line\nsec line\033[22mout"};
    ::styleEachLineIndividually(str, "\033[1m", "\033[22m");
    assert(str == "\033[1mfirst line\033[22m\n\033[1msec line\033[22mout");

    std::cout << "`test::styleEachLineIndividually()` success case passed\n";
}

void initConf() {
    std::cout << "`test::initConf()` no failure cases\n";

    const fs::path mncConfAbs{dotConfigAbs / "mnc/conf.xml"};
    ::initConf(mncConfAbs);

    std::cout << "`test::initConf()` success cases did not error, "
                 "check `.testing_xdg_dirs/.config/` for correct conf file\n";
}

void initLibrary() {
    std::cout << "`test::initLibrary()` no failure cases\n";

    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    ::initLibrary(mncLibraryAbs);

    std::cout << "`test::initLibrary()` success cases did not error, "
                 "check `.testing_xdg_dirs/share/` for correct library file\n";
}

void readMncConf() {
    const fs::path mncConfAbs{dotConfigAbs / "mnc/conf.xml"};
    const ConfOpts confOpts{::readMncConf(mncConfAbs)};

    std::cout << "`test::readMncConf()` success cases and failure cases "
                 "require manual verifications, change the config file "
                 "in testing config to verify correct behavior in cases\n";
    std::cout << "`<line-length>` chars: " << confOpts.lineLength << '\n';
}

void getTruncatedSHA256Sum() {
    try {
        (void)::getTruncatedSHA256Sum("/nonexistent_file_xyz");
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "cmd did not exit properly: shasum");
    }
    std::cout << "`test::getTruncatedSHA256Sum()` failure cases passed\n";

    assert(::getTruncatedSHA256Sum(epubsAbs / "lord_of_mysteries_vol_1.epub")
           == "53760b7bdcdfa01a43ccf243f41dd912");
    assert(::getTruncatedSHA256Sum(epubsAbs / "parasite_in_love.epub")
           == "40c5f7dce4a5576956a094eb7fb18cf8");
    assert(::getTruncatedSHA256Sum(epubsAbs / "spice_and_wolf_vol_1.epub")
           == "a6ce475b738e1cd7def7ed1e958426b3");
    assert(::getTruncatedSHA256Sum(epubsAbs / "zuttomo_vol_1.epub")
           == "8d070ee9c3292df13dfb8471f099565a");

    std::cout << "`test::getTruncatedSHA256Sum()` success cases passed\n";
}

void addToLibrary() {
    std::cout << "`test::addToLibrary()` no failure cases\n";

    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    ::initLibrary(mncLibraryAbs);

    const fs::path zippedEpubAbs{epubsAbs / "lord_of_mysteries_vol_1.epub"};
    ::addToLibrary(zippedEpubAbs, shareAbs);

    std::cout << "`test::addToLibrary()` success cases did not error, "
                 "check `.testing_xdg_dirs/share/` for correct library file "
                 "and extracted epub\n";
}

void deleteFromLibrary() {
    std::cout << "`test::deleteFromLibrary()` no failure cases\n";

    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    ::initLibrary(mncLibraryAbs);

    const fs::path zippedEpubAbs{epubsAbs / "lord_of_mysteries_vol_1.epub"};
    ::addToLibrary(zippedEpubAbs, shareAbs);

    const std::string id{"53760b7bdcdfa01a43ccf243f41dd912"};
    assert(::deleteFromLibrary(id, shareAbs));
    assert(!::deleteFromLibrary(id, shareAbs));

    std::cout << "`test::deleteFromLibrary()` success cases did not error, "
                 "check `.testing_xdg_dirs/share/` for correct library file "
                 "and removed extracted epub\n";
}
} // namespace test
