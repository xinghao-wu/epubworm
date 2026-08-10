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
const fs::path projectRootAbs{fs::current_path().parent_path()};

const fs::path epubsAbs{projectRootAbs / "test_epubs"};
const fs::path mysteriesRootAbs{epubsAbs / "lord_of_mysteries_vol_1_unzipped"};
const fs::path parasiteRootAbs{epubsAbs / "parasite_in_love_unzipped"};
const fs::path spiceWolfRootAbs{epubsAbs / "spice_and_wolf_vol_1_unzipped"};
const fs::path zuttomoRootAbs{epubsAbs / "zuttomo_vol_1_unzipped"};

const fs::path testOutputsAbs{projectRootAbs / "test_outputs"};

void unzip() {
    const fs::path mysteriesZippedAbs{epubsAbs
                                      / "lord_of_mysteries_vol_1.epub"};
    const fs::path parasiteZippedAbs{epubsAbs / "parasite_in_love.epub"};
    const fs::path spiceWolfZippedAbs{epubsAbs / "spice_and_wolf_vol_1.epub"};
    const fs::path zuttomoZippedAbs{epubsAbs / "zuttomo_vol_1.epub"};

    try {
        ::unzip("/bad archive path", testOutputsAbs);
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

    fs::create_directories(testOutputsAbs);
    ::unzip(mysteriesZippedAbs,
            testOutputsAbs / "unzip_lord_of_mysteries_vol_1_unzipped");
    ::unzip(parasiteZippedAbs,
            testOutputsAbs / "unzip_parasite_in_love_unzipped");
    ::unzip(spiceWolfZippedAbs,
            testOutputsAbs / "unzip_spice_and_wolf_vol_1_unzipped");
    ::unzip(zuttomoZippedAbs, testOutputsAbs / "unzip_zuttomo_vol_1_unzipped");
    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::unzip()` success cases need verification, "
                 "check `test_outputs/` to verify correct result\n";
    resetBoldColorIfTerm(stdout);
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

    assert(::getOPFRel(mysteriesRootAbs) == "content.opf");
    assert(::getOPFRel(parasiteRootAbs) == "OEBPS/content.opf");
    assert(::getOPFRel(spiceWolfRootAbs) == "content.opf");
    assert(::getOPFRel(zuttomoRootAbs) == "content.opf");
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

    assert(std::string_view{mysteriesMetadata->FirstChildElement("dc:language")
                                    ->GetText()}
           == "en");
    assert(std::string_view{parasiteMetadata->FirstChildElement("dc:publisher")
                                    ->GetText()}
           == "ASCII Media Works");
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

    assert(::getTitle(mysteriesMetadata)
           == "Lord of Mysteries Volume 1: Clown");
    assert(::getTitle(spiceWolfMetadata) == "Spice and Wolf, Vol. 1");
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

    assert(::getAuthor(mysteriesMetadata)
           == "Cuttlefish That Loves Diving (爱潜水的乌贼)");
    assert(::getAuthor(spiceWolfMetadata) == "Isuna Hasekura");
}

void findAndReplaceAll() {

    std::string hasTarget{"target, target on the wall,"};
    ::findAndReplaceAll(hasTarget, "target", "mirror");
    assert(hasTarget == "mirror, mirror on the wall,");

    std::string noTarget{"am I the fairest of them all?"};
    ::findAndReplaceAll(noTarget, "target", "mirror");
    assert(noTarget == "am I the fairest of them all?");
}

void wrapForTmuxPassthrough() {

    std::string notEmpty{"\033]1337;SetProfile=NewProfileName\007"};
    ::wrapForTmuxPassthrough(notEmpty);
    assert(notEmpty
           == "\033Ptmux;\033\033]1337;SetProfile=NewProfileName\007\033\\");

    std::string empty{};
    ::wrapForTmuxPassthrough(empty);
    assert(empty == "\033Ptmux;\033\\");
}

void displayImg() {
    std::string output{};

    try {
        ::displayImg("/bad img path", output);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()} == "Unable to open file");
    }

    ::displayImg(mysteriesRootAbs / "images/Tarot Club - V01B - Justice.jpg",
                 output);
    ::displayImg(parasiteRootAbs / "OEBPS/Images/cover.jpg", output);
    ::displayImg(parasiteRootAbs / "OEBPS/Images/ascii.png", output);
    ::displayImg(spiceWolfRootAbs / "OEBPS/images/Art_P6.jpg", output);
    ::displayImg(zuttomoRootAbs / "images/image1.jpeg", output);
    ::displayImg(zuttomoRootAbs / "images/image3.png", output);
    ::displayImg(zuttomoRootAbs / "images/image3.png", output, 35, 80);
    std::cout << output;

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::displayImg()` success cases need verification, "
                 "check `std::cout` for expected images\n";
    resetBoldColorIfTerm(stdout);
}

void getSpine() {

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
}

void getTOC() {

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
}

void parseChapter() {

    std::string result{};
    ::parseChapter(spiceWolfRootAbs / "OEBPS/chap01.xhtml", result);
    ::parseChapter(spiceWolfRootAbs / "OEBPS/chap02.xhtml", result);
    ::parseChapter(spiceWolfRootAbs / "OEBPS/chapter005.xhtml", result);
    std::cout << result;

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::parseChapter()` success cases need verification, "
                 "check `std::cout` for correct images and text\n";
    resetBoldColorIfTerm(stdout);
}

void dumpEpub() {

    useSystemLocale();

    std::string result{};
    ::dumpEpub(parasiteRootAbs, result);
    ::dumpEpub(spiceWolfRootAbs, result);
    ::dumpEpub(zuttomoRootAbs, result);
    ::processContentText(result, 55);
    std::cout << result;

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::dumpEpub()` success cases need verification, "
                 "check `std::cout` for content of three epubs. "
                 "Ellipses should be expanded, lines wrapped to 55 cols, "
                 "chapter titles and ends center justified, "
                 "and content centered on the screen.\n";
    resetBoldColorIfTerm(stdout);
}

void readRawInput() {

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::readRawInput()` success cases need verification, "
                 "the terminal should now be in raw mode. "
                 "Verify correct key, row, col values for inputs. "
                 "Press q to quit.\n";
    resetBoldColorIfTerm(stdout);

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

    assert(::utf8ToWide("hallo") == L"hallo");
    assert(::utf8ToWide("Hello, 世界") == L"Hello, 世界");
    assert(::utf8ToWide("Hello, World! 🚀") == L"Hello, World! 🚀");
}

void wideToUTF8() {

    assert(::wideToUTF8(L"hallo") == "hallo");
    assert(::wideToUTF8(L"Hello, 世界") == "Hello, 世界");
    assert(::wideToUTF8(L"Hello, World! 🚀") == "Hello, World! 🚀");
}

void findNth() {

    static_assert(::findNth("banana", "an", 2) == 3);
    static_assert(::findNth("mirra mirra on ze walle", "mirra", 1, 3) == 6);
    static_assert(::findNth("mirra mirra", "mirra", 3)
                  == std::string_view::npos);
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

    assert(::execute(std::vector<std::string>{"true"}).empty());
    assert(::execute(std::vector<std::string>{"echo", "hello"}) == "hello\n");
    assert(::execute(std::vector<std::string>{"printf", "a\\nb\\nc\\n"})
           == "a\nb\nc\n");

    const std::string largeOutput{
            ::execute(std::vector<std::string>{"seq", "50000"})};
    assert(largeOutput.size() > 65536);
    assert(largeOutput.starts_with("1\n"));
    assert(largeOutput.ends_with("50000\n"));
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

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::displayChapter()` success cases need verification, "
                 "a tui interface for an image and text "
                 "chapter should have been displayed.\n";
    resetBoldColorIfTerm(stdout);

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

    ::useSystemLocale();

    const TocData mysteriesTOC{::getTOC(mysteriesRootAbs / "toc.ncx")};
    const TocData parasiteTOC{::getTOC(parasiteRootAbs / "OEBPS/toc.ncx")};

    std::string str{};
    ::tocDataToString(mysteriesTOC, str);
    ::tocDataToString(parasiteTOC, str);
    ::processContentText(str, 55);
    std::cout << str;

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::tocDataToString()` success cases need verification, "
                 "verify correct table of content strings are shown\n";
    resetBoldColorIfTerm(stdout);
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

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::displayTOC()` success cases need verification, "
                 "a tui interface for mysteries' "
                 "and parasite's TOCs should have been displayed.\n";
    resetBoldColorIfTerm(stdout);

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

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::displayEpub()` success cases need verification, "
                 "a tui interface for all four "
                 "test epubs should have been displayed.\n";
    resetBoldColorIfTerm(stdout);

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

    std::string str{"\033[1mfirst line\nsec line\033[22mout"};
    ::styleEachLineIndividually(str, "\033[1m", "\033[22m");
    assert(str == "\033[1mfirst line\033[22m\n\033[1msec line\033[22mout");
}

void initConf() {
    const fs::path tmpConfigAbs{fs::temp_directory_path()
                                / "mnc_test_initConf"};
    fs::remove_all(tmpConfigAbs);
    fs::create_directories(tmpConfigAbs);

    const fs::path mncConfAbs{tmpConfigAbs / "mnc/conf.xml"};
    ::initConf(mncConfAbs);

    assert(fs::exists(mncConfAbs));

    XMLDocument mncConf{};
    assert(mncConf.LoadFile(mncConfAbs.c_str()) == XML_SUCCESS);
    assert(mncConf.FirstChildElement("conf") != nullptr);
    assert(mncConf.FirstChildElement("conf")->FirstChildElement() == nullptr);

    fs::remove_all(tmpConfigAbs);
}

void initLibrary() {
    const fs::path tmpShareAbs{fs::temp_directory_path()
                               / "mnc_test_initLibrary"};
    fs::remove_all(tmpShareAbs);
    fs::create_directories(tmpShareAbs);

    const fs::path mncLibraryAbs{tmpShareAbs / "mnc/library.xml"};
    ::initLibrary(mncLibraryAbs);

    assert(fs::exists(mncLibraryAbs));

    XMLDocument mncLibrary{};
    assert(mncLibrary.LoadFile(mncLibraryAbs.c_str()) == XML_SUCCESS);
    const XMLElement* const libraryRoot{
            mncLibrary.FirstChildElement("library")};
    assert(libraryRoot != nullptr);
    const XMLElement* const lastRead{
            libraryRoot->FirstChildElement("last-read")};
    assert(lastRead != nullptr);
    assert(lastRead->NextSiblingElement() == nullptr);
    const char* const id{lastRead->Attribute("id")};
    assert(id != nullptr);
    assert(std::string_view{id}.empty());

    fs::remove_all(tmpShareAbs);
}

void readMncConf() {
    const fs::path tmpConfigAbs{fs::temp_directory_path()
                                / "mnc_test_readMncConf"};
    fs::remove_all(tmpConfigAbs);
    fs::create_directories(tmpConfigAbs);

    const fs::path mncConfAbs{tmpConfigAbs / "mnc/conf.xml"};
    ::initConf(mncConfAbs);
    assert(::readMncConf(mncConfAbs).lineLength == 55);

    fs::remove_all(tmpConfigAbs);
}

void getTruncatedSHA256Sum() {

    assert(::getTruncatedSHA256Sum(epubsAbs / "lord_of_mysteries_vol_1.epub")
           == "53760b7bdcdfa01a43ccf243f41dd912");
    assert(::getTruncatedSHA256Sum(epubsAbs / "parasite_in_love.epub")
           == "40c5f7dce4a5576956a094eb7fb18cf8");
    assert(::getTruncatedSHA256Sum(epubsAbs / "spice_and_wolf_vol_1.epub")
           == "a6ce475b738e1cd7def7ed1e958426b3");
    assert(::getTruncatedSHA256Sum(epubsAbs / "zuttomo_vol_1.epub")
           == "8d070ee9c3292df13dfb8471f099565a");
}

void findEpubById() {
    // Three ids: the first two share the `aaaa` prefix, the third does not.
    constexpr std::string_view idA{"aaaa1111111111111111111111111111"};
    constexpr std::string_view idB{"aaaa2222222222222222222222222222"};
    constexpr std::string_view idC{"bbbb3333333333333333333333333333"};

    XMLDocument library{};
    library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                  "<library>"
                  "<epub id=\"aaaa1111111111111111111111111111\"/>"
                  "<epub id=\"aaaa2222222222222222222222222222\"/>"
                  "<epub id=\"bbbb3333333333333333333333333333\"/>"
                  "</library>");
    assert(!library.Error());

    XMLElement* const libraryRoot{library.FirstChildElement("library")};
    assert(libraryRoot != nullptr);

    // Full id: unambiguous match for each.
    assert(::findEpubById(libraryRoot, idA) != nullptr);
    assert(std::string_view{::findEpubById(libraryRoot, idA)->Attribute("id")}
           == idA);
    assert(::findEpubById(libraryRoot, idB) != nullptr);
    assert(std::string_view{::findEpubById(libraryRoot, idB)->Attribute("id")}
           == idB);
    assert(::findEpubById(libraryRoot, idC) != nullptr);
    assert(std::string_view{::findEpubById(libraryRoot, idC)->Attribute("id")}
           == idC);

    // Unique prefix: unambiguous match.
    assert(::findEpubById(libraryRoot, "aaaa1") != nullptr);
    assert(std::string_view{
                   ::findEpubById(libraryRoot, "aaaa1")->Attribute("id")}
           == idA);

    // Ambiguous prefix: matches both idA and idB -> nullptr.
    assert(::findEpubById(libraryRoot, "aaaa") == nullptr);

    // Empty prefix: matches all three -> nullptr.
    assert(::findEpubById(libraryRoot, "") == nullptr);

    // No-match prefix: nullptr.
    assert(::findEpubById(libraryRoot, "cccc") == nullptr);
}

void addToLibrary() {
    const fs::path tmpShareAbs{fs::temp_directory_path()
                               / "mnc_test_addToLibrary"};
    fs::remove_all(tmpShareAbs);
    fs::create_directories(tmpShareAbs);

    const fs::path mncLibraryAbs{tmpShareAbs / "mnc/library.xml"};
    ::initLibrary(mncLibraryAbs);

    assert(::addToLibrary(epubsAbs / "lord_of_mysteries_vol_1.epub",
                          tmpShareAbs));

    const std::string mysteriesId{"53760b7bdcdfa01a43ccf243f41dd912"};
    XMLDocument mncLibrary{};
    assert(mncLibrary.LoadFile(mncLibraryAbs.c_str()) == XML_SUCCESS);
    const XMLElement* const libraryRoot{
            mncLibrary.FirstChildElement("library")};
    assert(libraryRoot != nullptr);

    const XMLElement* const lastRead{
            libraryRoot->FirstChildElement("last-read")};
    assert(lastRead != nullptr);
    const char* const lastReadId{lastRead->Attribute("id")};
    assert(lastReadId != nullptr);
    assert(std::string_view{lastReadId} == mysteriesId);

    const XMLElement* const epub{libraryRoot->FirstChildElement("epub")};
    assert(epub != nullptr);
    assert(epub->NextSiblingElement("epub") == nullptr);
    assert(std::string_view{epub->Attribute("id")} == mysteriesId);
    assert(std::string_view{epub->Attribute("opened-chapter")}.empty());
    assert(epub->DoubleAttribute("chapter-progress") == 0.0);

    assert(fs::is_directory(tmpShareAbs / "mnc/extracted_epubs"
                            / mysteriesId));

    // Already present: returns false.
    assert(!::addToLibrary(epubsAbs / "lord_of_mysteries_vol_1.epub",
                           tmpShareAbs));

    fs::remove_all(tmpShareAbs);
}

void queryEpubElem() {
    const fs::path phonyShareAbs{"/phony_share"};
    const std::string_view idA{"53760b7bdcdfa01a43ccf243f41dd912"};
    const std::string_view idB{"40c5f7dce4a5576956a094eb7fb18cf8"};

    XMLDocument library{};
    library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                  "<library>"
                  "<last-read id=\"53760b7bdcdfa01a43ccf243f41dd912\"/>"
                  "<epub id=\"53760b7bdcdfa01a43ccf243f41dd912\" "
                  "opened-chapter=\"index_split_117.html\" "
                  "chapter-progress=\"0.5\"/>"
                  "<epub id=\"40c5f7dce4a5576956a094eb7fb18cf8\" "
                  "opened-chapter=\"\" chapter-progress=\"0\"/>"
                  "</library>");
    assert(!library.Error());

    XMLElement* const libraryRoot{library.FirstChildElement("library")};
    assert(libraryRoot != nullptr);

    // Success case A: non-empty opened-chapter, non-zero progress.
    const XMLElement* const epubA{::findEpubById(libraryRoot, idA)};
    assert(epubA != nullptr);
    const std::pair<std::string, EpubProg> outA{
            ::queryEpubElem(epubA, phonyShareAbs)};
    assert(outA.first == idA);
    assert(outA.second.chapterAbs
           == phonyShareAbs / "mnc/extracted_epubs" / idA
                      / "index_split_117.html");
    assert(outA.second.chapterProg == 0.5);

    // Success case B: empty opened-chapter -> empty chapterAbs, zero progress.
    const XMLElement* const epubB{::findEpubById(libraryRoot, idB)};
    assert(epubB != nullptr);
    const std::pair<std::string, EpubProg> outB{
            ::queryEpubElem(epubB, phonyShareAbs)};
    assert(outB.first == idB);
    assert(outB.second.chapterAbs.empty());
    assert(outB.second.chapterProg == 0.0);

    // Failure case: missing `id` attribute.
    XMLDocument badLibrary{};
    badLibrary.Parse(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<library>"
            "<epub opened-chapter=\"x.html\" chapter-progress=\"0.1\"/>"
            "</library>");
    assert(!badLibrary.Error());
    XMLElement* const badRoot{badLibrary.FirstChildElement("library")};
    assert(badRoot != nullptr);
    const XMLElement* const badEpub{badRoot->FirstChildElement("epub")};
    assert(badEpub != nullptr);

    try {
        (void)::queryEpubElem(badEpub, phonyShareAbs);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "epub entry missing `id` attribute");
    }

    // Failure case: missing `chapter-progress` attribute.
    XMLDocument noProgLibrary{};
    noProgLibrary.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<library>"
                        "<epub id=\"abc\" opened-chapter=\"x.html\"/>"
                        "</library>");
    assert(!noProgLibrary.Error());
    const XMLElement* const noProgEpub{
            noProgLibrary.FirstChildElement("library")->FirstChildElement(
                    "epub")};
    assert(noProgEpub != nullptr);

    try {
        (void)::queryEpubElem(noProgEpub, phonyShareAbs);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "epub entry missing `chapter-progress` attribute");
    }

    // Failure case: `chapter-progress` not a valid double.
    XMLDocument badProgLibrary{};
    badProgLibrary.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                         "<library>"
                         "<epub id=\"abc\" opened-chapter=\"x.html\" "
                         "chapter-progress=\"not-a-number\"/>"
                         "</library>");
    assert(!badProgLibrary.Error());
    const XMLElement* const badProgEpub{
            badProgLibrary.FirstChildElement("library")->FirstChildElement(
                    "epub")};
    assert(badProgEpub != nullptr);

    try {
        (void)::queryEpubElem(badProgEpub, phonyShareAbs);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "epub entry has invalid `chapter-progress` attribute");
    }
}

void writeProgress() {
    const fs::path phonyShareAbs{"/phony_share"};
    const std::string_view id{"53760b7bdcdfa01a43ccf243f41dd912"};

    XMLDocument library{};
    library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                  "<library>"
                  "<epub id=\"53760b7bdcdfa01a43ccf243f41dd912\" "
                  "opened-chapter=\"\" chapter-progress=\"0\"/>"
                  "</library>");
    assert(!library.Error());
    XMLElement* const libraryRoot{library.FirstChildElement("library")};
    assert(libraryRoot != nullptr);
    XMLElement* const epub{libraryRoot->FirstChildElement("epub")};
    assert(epub != nullptr);

    // Success case A: non-empty chapterAbs, non-zero progress.
    const EpubProg prog{phonyShareAbs / "mnc/extracted_epubs" / id
                                / "index_split_117.html",
                        0.5};
    ::writeProgress(epub, prog, phonyShareAbs);
    assert(std::string_view{epub->Attribute("opened-chapter")}
           == "index_split_117.html");
    assert(epub->DoubleAttribute("chapter-progress") == 0.5);

    // Round-trip: queryEpubElem should reconstruct the original prog.
    const std::pair<std::string, EpubProg> out{
            ::queryEpubElem(epub, phonyShareAbs)};
    assert(out.second.chapterAbs == prog.chapterAbs);
    assert(out.second.chapterProg == 0.5);

    // Success case B: empty chapterAbs -> empty opened-chapter, zero progress.
    const EpubProg emptyProg{};
    ::writeProgress(epub, emptyProg, phonyShareAbs);
    assert(std::string_view{epub->Attribute("opened-chapter")}.empty());
    assert(epub->DoubleAttribute("chapter-progress") == 0.0);

    // Failure case: missing `id` attribute.
    XMLDocument badLibrary{};
    badLibrary.Parse(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<library>"
            "<epub opened-chapter=\"x.html\" chapter-progress=\"0.1\"/>"
            "</library>");
    assert(!badLibrary.Error());
    const XMLElement* const badEpub{
            badLibrary.FirstChildElement("library")->FirstChildElement(
                    "epub")};
    assert(badEpub != nullptr);

    try {
        ::writeProgress(const_cast<XMLElement*>(badEpub), prog, phonyShareAbs);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "epub entry missing `id` attribute");
    }
}

void deleteFromLibrary() {
    const fs::path tmpShareAbs{fs::temp_directory_path()
                               / "mnc_test_deleteFromLibrary"};
    fs::remove_all(tmpShareAbs);
    fs::create_directories(tmpShareAbs);

    const fs::path mncLibraryAbs{tmpShareAbs / "mnc/library.xml"};
    ::initLibrary(mncLibraryAbs);

    assert(::addToLibrary(epubsAbs / "lord_of_mysteries_vol_1.epub",
                          tmpShareAbs));

    const std::string mysteriesID{"53760b7bdcdfa01a43ccf243f41dd912"};
    assert(::deleteFromLibrary(mysteriesID, tmpShareAbs));

    XMLDocument mncLibrary{};
    assert(mncLibrary.LoadFile(mncLibraryAbs.c_str()) == XML_SUCCESS);
    const XMLElement* const libraryRoot{
            mncLibrary.FirstChildElement("library")};
    assert(libraryRoot != nullptr);
    assert(libraryRoot->FirstChildElement("epub") == nullptr);
    const XMLElement* const lastRead{
            libraryRoot->FirstChildElement("last-read")};
    assert(lastRead != nullptr);
    const char* const id{lastRead->Attribute("id")};
    assert(id != nullptr);
    assert(std::string_view{id}.empty());

    assert(!fs::exists(tmpShareAbs / "mnc/extracted_epubs" / mysteriesID));

    // Already removed: returns false.
    assert(!::deleteFromLibrary(mysteriesID, tmpShareAbs));

    fs::remove_all(tmpShareAbs);
}

void readEpubInLibrary() {
    const fs::path tmpShareAbs{fs::temp_directory_path()
                               / "mnc_test_readEpubInLibrary"};
    fs::remove_all(tmpShareAbs);
    fs::create_directories(tmpShareAbs);

    const fs::path shareMncAbs{tmpShareAbs / "mnc"};
    fs::create_directories(shareMncAbs);

    const fs::path mncLibraryAbs{shareMncAbs / "library.xml"};
    ::initLibrary(mncLibraryAbs);

    assert(::addToLibrary(epubsAbs / "lord_of_mysteries_vol_1.epub",
                          tmpShareAbs));
    assert(::addToLibrary(epubsAbs / "parasite_in_love.epub", tmpShareAbs));
    assert(::addToLibrary(epubsAbs / "zuttomo_vol_1.epub", tmpShareAbs));
    assert(::addToLibrary(epubsAbs / "spice_and_wolf_vol_1.epub",
                          tmpShareAbs));

    assert(!::readEpubInLibrary("000000", tmpShareAbs, 55));
    assert(!::readEpubInLibrary("", tmpShareAbs, 55));

    assert(::readEpubInLibrary("53760b", tmpShareAbs, 50));
    assert(::readEpubInLibrary("40c5f7", tmpShareAbs, 55));
    assert(::readEpubInLibrary("a6ce47", tmpShareAbs, 60));
    assert(::readEpubInLibrary("8d070e", tmpShareAbs, 65));

    fs::create_directories(testOutputsAbs);
    const fs::path outputMncLibraryAbs{testOutputsAbs
                                       / "readEpubInLibrary_library.xml"};
    fs::copy_file(mncLibraryAbs, outputMncLibraryAbs,
                  fs::copy_options::overwrite_existing);

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "`test::readEpubInLibrary()` success cases need "
                 "verification.\n"
                 "Confirm a working TUI interface for all test epubs "
                 "were displayed.\n"
                 "Check `test_outputs/readEpubInLibrary_library.xml` "
                 "for correct progresses and `<last-read>`.\n";
    resetBoldColorIfTerm(stdout);

    fs::remove_all(tmpShareAbs);
}

void getLastRead() {
    XMLDocument library{};
    library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                  "<library>"
                  "<last-read id=\"53760b7bdcdfa01a43ccf243f41dd912\"/>"
                  "</library>");
    assert(!library.Error());
    assert(::getLastRead(library) == "53760b7bdcdfa01a43ccf243f41dd912");

    XMLDocument noLibraryRoot{};
    noLibraryRoot.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<not-library/>");
    assert(!noLibraryRoot.Error());
    try {
        (void)::getLastRead(noLibraryRoot);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "root element `<library>` missing in library file");
    }

    XMLDocument noLastRead{};
    noLastRead.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                     "<library>"
                     "<epub id=\"abc\"/>"
                     "</library>");
    assert(!noLastRead.Error());
    try {
        (void)::getLastRead(noLastRead);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "`<last-read>` element missing in library file");
    }

    XMLDocument noId{};
    noId.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
               "<library>"
               "<last-read/>"
               "</library>");
    assert(!noId.Error());
    try {
        (void)::getLastRead(noId);
        assert(false);
    } catch (const std::runtime_error& e) {
        assert(std::string_view{e.what()}
               == "`<last-read>` element missing `id` attribute");
    }
}
} // namespace test
