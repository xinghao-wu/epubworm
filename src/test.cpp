#include "test.hpp"
#include "data_management.hpp"
#include "epub_parser.hpp"
#include "row_col_diacritics.hpp"
#include "single_instance.hpp"
#include "tinyxml2/tinyxml2.hpp"
#include "tui.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
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

const fs::path epubsAbs{projectRootAbs / "fixtures/epubs"};
const fs::path metadataPathsRootAbs{epubsAbs / "metadata_paths"};
const fs::path nonlinearSpineRootAbs{epubsAbs / "nonlinear_spine"};
const fs::path imageElementsRootAbs{epubsAbs / "image_elements"};
const fs::path nestedNavigationRootAbs{epubsAbs / "nested_navigation"};

const fs::path testOutputsAbs{projectRootAbs / "test_outputs"};

auto singleInstanceLock() -> void {
  const fs::path tmpLockAbs{fs::temp_directory_path()
                            / "epubworm_test_singleInstanceLock"};
  fs::remove(tmpLockAbs);

  {
    const SingleInstanceLock firstLock{tmpLockAbs};
    assert(firstLock.isFirstInstance());

    const SingleInstanceLock secondLock{tmpLockAbs};
    assert(!secondLock.isFirstInstance());
  }

  {
    const SingleInstanceLock reacquiredLock{tmpLockAbs};
    assert(reacquiredLock.isFirstInstance());
  }

  fs::remove(tmpLockAbs);
}

auto unzip() -> void {
  const fs::path metadataPathsZippedAbs{epubsAbs / "metadata_paths.epub"};
  const fs::path nonlinearSpineZippedAbs{epubsAbs / "nonlinear_spine.epub"};
  const fs::path imageElementsZippedAbs{epubsAbs / "image_elements.epub"};
  const fs::path nestedNavigationZippedAbs{epubsAbs / "nested_navigation.epub"};

  try {
    ::unzip("/bad archive path", testOutputsAbs);
    assert(false);
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()} == "bad zip");
  }
  try {
    ::unzip(nonlinearSpineZippedAbs, "/bad destination path");
    assert(false);
  }
  catch (const fs::filesystem_error& e) {
    assert(std::string_view{e.what()}
           == "filesystem error: cannot create directories: "
              "Permission denied [/bad destination path/META-INF]");
  }

  fs::create_directories(testOutputsAbs);
  ::unzip(metadataPathsZippedAbs, testOutputsAbs / "unzip_metadata_paths");
  ::unzip(nonlinearSpineZippedAbs, testOutputsAbs / "unzip_nonlinear_spine");
  ::unzip(imageElementsZippedAbs, testOutputsAbs / "unzip_image_elements");
  ::unzip(nestedNavigationZippedAbs,
          testOutputsAbs / "unzip_nested_navigation");
  boldColorIfTerm(stdout, yellowFG);
  std::cout << "`test::unzip()` success cases need verification, "
               "check `test_outputs/` to verify correct result\n";
  resetBoldColorIfTerm(stdout);
}

auto getOPFRel() -> void {
  try {
    (void)::getOPFRel("/bad epub root");
    assert(false);
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()}
           == "Error=XML_ERROR_FILE_NOT_FOUND "
              "ErrorID=3 (0x3) Line number=0: "
              "filename=/bad epub root/META-INF/container.xml");
  }

  assert(::getOPFRel(metadataPathsRootAbs) == "content.opf");
  assert(::getOPFRel(nonlinearSpineRootAbs) == "OEBPS/content.opf");
  assert(::getOPFRel(imageElementsRootAbs) == "content.opf");
  assert(::getOPFRel(nestedNavigationRootAbs) == "Book/package.opf");
}

auto getMetadata() -> void {
  XMLDocument metadataPathsOPF{};
  metadataPathsOPF.LoadFile(
      (metadataPathsRootAbs / ::getOPFRel(metadataPathsRootAbs)).c_str());
  XMLDocument nonlinearSpineOPF{};
  nonlinearSpineOPF.LoadFile(
      (nonlinearSpineRootAbs / ::getOPFRel(nonlinearSpineRootAbs)).c_str());

  const XMLElement* metadataPathsMetadata{::getMetadata(metadataPathsOPF)};
  const XMLElement* nonlinearSpineMetadata{::getMetadata(nonlinearSpineOPF)};

  assert(std::string_view{
             metadataPathsMetadata->FirstChildElement("dc:language")->GetText()}
         == "en");
  assert(
      std::string_view{
          nonlinearSpineMetadata->FirstChildElement("dc:publisher")->GetText()}
      == "Nested Fixture Press");
}

auto getTitle() -> void {
  XMLDocument metadataPathsOPF{};
  metadataPathsOPF.LoadFile(
      (metadataPathsRootAbs / ::getOPFRel(metadataPathsRootAbs)).c_str());
  XMLDocument imageElementsOPF{};
  imageElementsOPF.LoadFile(
      (imageElementsRootAbs / ::getOPFRel(imageElementsRootAbs)).c_str());

  const XMLElement* metadataPathsMetadata{::getMetadata(metadataPathsOPF)};
  const XMLElement* imageElementsMetadata{::getMetadata(imageElementsOPF)};

  assert(::getTitle(metadataPathsMetadata) == "The Clockwork Garden");
  assert(::getTitle(imageElementsMetadata) == "Illustrated Signals");
}

auto getAuthor() -> void {
  XMLDocument metadataPathsOPF{};
  metadataPathsOPF.LoadFile(
      (metadataPathsRootAbs / ::getOPFRel(metadataPathsRootAbs)).c_str());
  XMLDocument imageElementsOPF{};
  imageElementsOPF.LoadFile(
      (imageElementsRootAbs / ::getOPFRel(imageElementsRootAbs)).c_str());

  const XMLElement* metadataPathsMetadata{::getMetadata(metadataPathsOPF)};
  const XMLElement* imageElementsMetadata{::getMetadata(imageElementsOPF)};

  assert(::getAuthor(metadataPathsMetadata) == "Epubworm Project");
  assert(::getAuthor(imageElementsMetadata) == "Fixture Workshop");
}

auto findAndReplaceAll() -> void {

  std::string hasTarget{"target, target on the wall,"};
  ::findAndReplaceAll(hasTarget, "target", "mirror");
  assert(hasTarget == "mirror, mirror on the wall,");

  std::string noTarget{"am I the fairest of them all?"};
  ::findAndReplaceAll(noTarget, "target", "mirror");
  assert(noTarget == "am I the fairest of them all?");
}

auto wrapForTmuxPassthrough() -> void {

  std::string notEmpty{"\033]1337;SetProfile=NewProfileName\007"};
  ::wrapForTmuxPassthrough(notEmpty);
  assert(notEmpty
         == "\033Ptmux;\033\033]1337;SetProfile=NewProfileName\007\033\\");

  std::string empty{};
  ::wrapForTmuxPassthrough(empty);
  assert(empty == "\033Ptmux;\033\\");
}

auto wrapLines() -> void {
  std::string breakable{"one two three"};
  ::wrapLines(breakable, 7);
  assert(breakable == "one two\nthree");

  std::string unbreakable{"abcdefghij"};
  ::wrapLines(unbreakable, 4);
  assert(unbreakable == "abcd\nefgh\nij");

  std::string markedUnbreakable{"abcdefghij"};
  ::wrapLines(markedUnbreakable, 4, true);
  assert(markedUnbreakable
         == "abcd" + forcedWrapMarker + "\nefgh" + forcedWrapMarker + "\nij");

  std::string longFirstWord{"abcdefgh ij"};
  ::wrapLines(longFirstWord, 4);
  assert(longFirstWord == "abcd\nefgh\nij");

  std::string utf8Unbreakable{"春夏秋冬"};
  ::wrapLines(utf8Unbreakable, 4);
  assert(utf8Unbreakable == "春夏\n秋冬");

  std::string styled{esc + cyanFG + "abcdefgh" + esc + resetFG};
  ::wrapLines(styled, 4);
  assert(styled == esc + cyanFG + "abcd\nefgh" + esc + resetFG);

  std::string imageLine{};
  ::displayLoadedImg(0x010203, 1, 8, imageLine, false);
  const std::string originalImageLine{imageLine};
  ::wrapLines(imageLine, 4);
  assert(imageLine == originalImageLine);
}

auto imageEscCodes() -> void {
  std::string compactOutput{};
  ::displayLoadedImg(0x010203, 2, 3, compactOutput, false);

  std::string expectedCompact{};
  for (int r{0}; r < 2; ++r) {
    expectedCompact += esc + "[38;2;1;2;3m";
    expectedCompact += imgCellPlaceholder + rowColDiacritics.data()[r];
    for (int c{1}; c < 3; ++c) {
      expectedCompact += imgCellPlaceholder;
    }
    expectedCompact += esc + resetFG + '\n';
  }
  assert(compactOutput == expectedCompact);

  std::string iTerm2Output{};
  ::displayLoadedImg(0x010203, 2, 3, iTerm2Output, true);

  std::string expectedITerm2{};
  for (int r{0}; r < 2; ++r) {
    expectedITerm2 += esc + "[38;2;1;2;3m";
    for (int c{0}; c < 3; ++c) {
      expectedITerm2 += imgCellPlaceholder + rowColDiacritics.data()[r]
                        + rowColDiacritics.data()[c] + rowColDiacritics.front();
    }
    expectedITerm2 += esc + resetFG + '\n';
  }
  assert(iTerm2Output == expectedITerm2);

  const std::string graphicsEscCode{::getGraphicsEscCode(
      "/tmp/test-tty-graphics-protocol", 4, 10, 20, 0x010203, 2, 3)};
  assert(graphicsEscCode.starts_with(
      "\033_Gf=32,s=10,v=20,i=66051,r=2,c=3,t=t,U=1,a=T,q=2;"));
  assert(graphicsEscCode.ends_with(escEnd));

  XMLDocument imageChapter{};
  assert(imageChapter.Parse(
             "<body>before<img src='../Images/accent.png'/>after</body>")
         == XML_SUCCESS);
  std::string parsedImage{};
  const std::ostringstream imageGraphicsOutput{};
  std::streambuf* originalCoutBuffer{
      std::cout.rdbuf(imageGraphicsOutput.rdbuf())};
  ::parseContentElem(imageChapter.FirstChildElement("body"), parsedImage,
                     nonlinearSpineRootAbs / "OEBPS/Text/test.xhtml");
  std::cout.rdbuf(originalCoutBuffer);

  assert(parsedImage.starts_with("before\n\n" + esc + "[38;2;"));
  assert(parsedImage.contains(imgCellPlaceholder));
  assert(parsedImage.ends_with(esc + resetFG + "\n\n\nafter"));

  const std::string imageGraphics{imageGraphicsOutput.str()};
  const std::size_t idMarker{imageGraphics.find("i=")};
  assert(idMarker != std::string::npos);
  const std::size_t idBegin{idMarker + 2};
  const std::size_t idEnd{imageGraphics.find(',', idBegin)};
  assert(idEnd != std::string::npos);
  const fs::path rawDataDirAbs{fs::exists("/dev/shm")
                                   ? fs::path{"/dev/shm"}
                                   : fs::temp_directory_path()};
  fs::remove(rawDataDirAbs
             / ("epubworm-img-data-"
                + imageGraphics.substr(idBegin, idEnd - idBegin)
                + "-tty-graphics-protocol"));
}

auto imageChannels() -> void {
  constexpr std::array<unsigned char, 4> sourcePixel{17, 34, 51, 68};
  constexpr std::uint32_t firstTestImageID{0xFFFFFFF0};
  constexpr std::array<std::string_view, 4> pngDataEncoded{
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVR4nGMQBAAAE"
      "wASpgy+1QAAAABJRU5ErkJggg==",
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGMQVAIAA"
      "EcANCQ5aoYAAAAASUVORK5CYII=",
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGMQVDIGA"
      "ACuAGcVHqFfAAAAAElFTkSuQmCC",
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMQVDJ2A"
      "QABWQCrEyolqwAAAABJRU5ErkJggg=="};
  const fs::path rawDataDirAbs{fs::exists("/dev/shm")
                                   ? fs::path{"/dev/shm"}
                                   : fs::temp_directory_path()};

  for (int sourceChannels{1}; sourceChannels <= 4; ++sourceChannels) {
    const std::string pngData{base64::from_base64(
        pngDataEncoded.at(static_cast<std::size_t>(sourceChannels - 1)))};

    const fs::path imageAbs{
        fs::temp_directory_path()
        / ("epubworm-test-" + std::to_string(sourceChannels) + "-channel.png")};
    std::ofstream imageFile{imageAbs, std::ios::binary};
    assert(imageFile.is_open());
    imageFile.write(pngData.data(),
                    static_cast<std::streamsize>(pngData.size()));
    imageFile.close();

    const std::uint32_t id{firstTestImageID
                           + static_cast<std::uint32_t>(sourceChannels)};
    const fs::path rawDataAbs{rawDataDirAbs
                              / ("epubworm-img-data-" + std::to_string(id)
                                 + "-tty-graphics-protocol")};

    const std::ostringstream graphicsOutput{};
    std::streambuf* originalCoutBuffer{std::cout.rdbuf(graphicsOutput.rdbuf())};
    ::loadImg(imageAbs, id, 1, 1);
    std::cout.rdbuf(originalCoutBuffer);

    const int loadedChannels{sourceChannels < 3 ? 3 : sourceChannels};
    assert(graphicsOutput.str().contains(
        "f=" + std::to_string(loadedChannels * 8) + ','));

    std::ifstream rawDataFile{rawDataAbs, std::ios::binary};
    const std::string rawData{std::istreambuf_iterator<char>{rawDataFile},
                              std::istreambuf_iterator<char>{}};
    if (sourceChannels < 3) {
      assert(rawData == std::string(3, static_cast<char>(sourcePixel[0])));
    }
    else {
      const std::string expectedRawData{
          reinterpret_cast<const char*>(sourcePixel.data()),
          static_cast<std::size_t>(sourceChannels)};
      assert(rawData == expectedRawData);
    }

    fs::remove(imageAbs);
    fs::remove(rawDataAbs);
  }
}

auto displayImg() -> void {
  std::string output{};

  try {
    ::displayImg("/bad img path", output);
    assert(false);
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()} == "Unable to open file");
  }

  ::displayImg(metadataPathsRootAbs / "images/garden cover.jpg", output);
  ::displayImg(nonlinearSpineRootAbs / "OEBPS/Images/cover.jpg", output);
  ::displayImg(nonlinearSpineRootAbs / "OEBPS/Images/accent.png", output);
  ::displayImg(imageElementsRootAbs / "Images/signal.jpg", output);
  ::displayImg(imageElementsRootAbs / "Images/pixel art.png", output);
  ::displayImg(nestedNavigationRootAbs / "Book/Images/map.png", output);
  ::displayImg(nestedNavigationRootAbs / "Book/Images/map.png", output, 35, 80);
  std::cout << output;

  boldColorIfTerm(stdout, yellowFG);
  std::cout << "`test::displayImg()` success cases need verification, "
               "check `std::cout` for expected images\n";
  resetBoldColorIfTerm(stdout);
}

auto getSpine() -> void {

  XMLDocument imageElementsOPF{};
  imageElementsOPF.LoadFile(
      (imageElementsRootAbs / ::getOPFRel(imageElementsRootAbs)).c_str());
  const std::vector<fs::path> imageElementsSpine{::getSpine(imageElementsOPF)};

  assert(imageElementsSpine.size() == 4);
  assert(imageElementsSpine[0] == "toc.ncx");
  assert(imageElementsSpine[1] == "Text/svg.xhtml");

  XMLDocument nonlinearSpineOPF{};
  nonlinearSpineOPF.LoadFile(
      (nonlinearSpineRootAbs / ::getOPFRel(nonlinearSpineRootAbs)).c_str());
  const std::vector<fs::path> nonlinearSpineSpine{
      ::getSpine(nonlinearSpineOPF)};

  assert(nonlinearSpineSpine.size() == 4);
  assert(nonlinearSpineSpine[0] == "toc.ncx");
  assert(nonlinearSpineSpine[1] == "Text/cover.xhtml");
  assert(nonlinearSpineSpine[2] == "Text/intro.xhtml");
  assert(nonlinearSpineSpine[3] == "Text/chapter.xhtml");
}

auto getTOC() -> void {

  const TocData metadataPathsTOC{::getTOC(metadataPathsRootAbs / "toc.ncx")};
  assert(metadataPathsTOC.size() == 3);
  assert(metadataPathsTOC[0].first == "Garden Gate");
  assert(metadataPathsTOC[0].second == "titlepage.xhtml");
  assert(metadataPathsTOC[1].first == "Chapter One");
  assert(metadataPathsTOC[1].second == "chapters/chapter one.xhtml");
  assert(metadataPathsTOC[2].first == "Afterword");
  assert(metadataPathsTOC[2].second == "chapters/afterword.xhtml");

  const TocData nonlinearSpineTOC{
      ::getTOC(nonlinearSpineRootAbs / "OEBPS/toc.ncx")};
  assert(nonlinearSpineTOC.size() == 3);
  assert(nonlinearSpineTOC[0].first == "Cover");
  assert(nonlinearSpineTOC[0].second == "Text/cover.xhtml");
  assert(nonlinearSpineTOC[2].first == "Inner Room");
  assert(nonlinearSpineTOC[2].second == "Text/chapter.xhtml");

  const TocData nestedNavigationTOC{
      ::getTOC(nestedNavigationRootAbs / "Book/toc.ncx")};
  assert(nestedNavigationTOC.size() == 3);
  assert(nestedNavigationTOC[0].first == "First Branch");
  assert(nestedNavigationTOC[0].second == "Text/one.xhtml");
  assert(nestedNavigationTOC[1].first == "    Second Branch");
  assert(nestedNavigationTOC[1].second == "Text/two.xhtml");
  assert(nestedNavigationTOC[2].first == "        Third Branch");
  assert(nestedNavigationTOC[2].second == "Text/three.xhtml");
}

auto parseChapter() -> void {

  std::string result{};
  ::parseChapter(imageElementsRootAbs / "Text/svg.xhtml", result);
  ::parseChapter(imageElementsRootAbs / "Text/html-image.xhtml", result);
  ::parseChapter(imageElementsRootAbs / "Text/prose.xhtml", result);
  std::cout << result;

  boldColorIfTerm(stdout, yellowFG);
  std::cout << "`test::parseChapter()` success cases need verification, "
               "check `std::cout` for correct images and text\n";
  resetBoldColorIfTerm(stdout);
}

auto dumpEpub() -> void {

  std::string result{};
  ::dumpEpub(nonlinearSpineRootAbs, result);
  ::dumpEpub(imageElementsRootAbs, result);
  ::dumpEpub(nestedNavigationRootAbs, result);
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

auto readRawInput() -> void {

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

auto utf8ToWide() -> void {

  assert(::utf8ToWide("hallo") == L"hallo");
  assert(::utf8ToWide("Hello, 世界") == L"Hello, 世界");
  assert(::utf8ToWide("Hello, World! 🚀") == L"Hello, World! 🚀");
}

auto wideToUTF8() -> void {

  assert(::wideToUTF8(L"hallo") == "hallo");
  assert(::wideToUTF8(L"Hello, 世界") == "Hello, 世界");
  assert(::wideToUTF8(L"Hello, World! 🚀") == "Hello, World! 🚀");
}

auto collapseConsecutiveNewlines() -> void {
  std::string unchanged{"zero\none\n\ntwo"};
  ::collapseConsecutiveNewlines(unchanged);
  assert(unchanged == "zero\none\n\ntwo");

  std::string collapsed{"\n\n\nstart\n\n\n\nmiddle\n\n\n\n\n"};
  ::collapseConsecutiveNewlines(collapsed);
  assert(collapsed == "\n\nstart\n\nmiddle\n\n");

  std::string whitespaceSeparated{"before \n \n\t\ncontent\n"
                                  "\xC2\xA0"
                                  "\n \t"
                                  "\xC2\xA0"
                                  "\xE2\x80\x8C"
                                  "\n"
                                  "\xE2\x80\x8C"
                                  "\n  after"};
  ::collapseConsecutiveNewlines(whitespaceSeparated);
  assert(whitespaceSeparated == "before \n\ncontent\n\n  after");

  std::string unboundedWhitespace{"\n \tcontent\n \t"
                                  "\xC2\xA0"
                                  "\xE2\x80\x8C"};
  const std::string expectedUnboundedWhitespace{unboundedWhitespace};
  ::collapseConsecutiveNewlines(unboundedWhitespace);
  assert(unboundedWhitespace == expectedUnboundedWhitespace);

  std::string embeddedNonJoiner{"before\xE2\x80\x8C"
                                "after"};
  const std::string expectedEmbeddedNonJoiner{embeddedNonJoiner};
  ::collapseConsecutiveNewlines(embeddedNonJoiner);
  assert(embeddedNonJoiner == expectedEmbeddedNonJoiner);
}

auto findNth() -> void {

  static_assert(::findNth("banana", "an", 2) == 3);
  static_assert(::findNth("mirra mirra on ze walle", "mirra", 1, 3) == 6);
  static_assert(::findNth("mirra mirra", "mirra", 3) == std::string_view::npos);
}

auto execute() -> void {
  try {
    ::execute(std::vector<std::string>{});
    assert(false);
  }
  catch (const std::invalid_argument& e) {
    assert(std::string_view{e.what()} == "execute() cmd cannot be empty");
  }
  try {
    ::execute(std::vector<std::string>{""});
    assert(false);
  }
  catch (const std::invalid_argument& e) {
    assert(std::string_view{e.what()} == "execute() cmd cannot be empty");
  }
  try {
    ::execute(std::vector<std::string>{"nonexistent_cmd_xyz"});
    assert(false);
  }
  catch (const std::system_error& e) {
    assert(std::string_view{e.what()}.starts_with("failed to spawn cmd: "));
  }
  try {
    ::execute(std::vector<std::string>{"false"});
    assert(false);
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()} == "cmd did not exit properly: false");
  }

  assert(::execute(std::vector<std::string>{"true"}).empty());
  assert(::execute(std::vector<std::string>{"echo", "hello"}) == "hello\n");
  assert(::execute(std::vector<std::string>{"printf", "a\\nb\\nc\\n"})
         == "a\nb\nc\n");

  const std::string expectedLargeOutput(70000, 'x');
  const std::string largeOutput{
      ::execute(std::vector<std::string>{"printf", "%s", expectedLargeOutput})};
  assert(largeOutput == expectedLargeOutput);
}

auto getChapterProgressIndicator() -> void {
  const auto assertStats = [](std::string_view chapter, int words,
                              int cjkCharacters) -> void {
    const ChapterReadingStats stats{::getChapterReadingStats(chapter)};
    assert(stats.words == words);
    assert(stats.cjkCharacters == cjkCharacters);
  };
  assertStats("one  \n two", 2, 0);
  assertStats(esc + yellowFG + "styled words" + esc + resetFG, 2, 0);
  assertStats("don't well-being", 2, 0);
  assertStats("super" + forcedWrapMarker + "\ncalifragilistic", 1, 0);
  assertStats("super" + forcedWrapMarker + esc + resetBold + "\n" + esc + bold
                  + "califragilistic",
              1, 0);
  assertStats("abc" + imgCellPlaceholder + "def", 2, 0);
  assertStats("你好世界", 0, 4);
  assertStats("日本語を読む", 0, 6);
  assertStats("hello 世界 and 日本語", 2, 5);
  assertStats("cafe\u0301 noir", 2, 0);
  assertStats("は\u3099", 0, 1);
  assertStats(esc + yellowFG + "styled 世界" + esc + resetFG, 1, 2);

  const ChapterReadingStats englishStats{.words = 1000};

  const auto expected = [](int percent, int minMinutes, int maxMinutes,
                           int cols) -> std::string {
    const std::string left{"─ Test Title "};
    const std::string progress{std::to_string(percent) + "% chapter progress"};
    const std::string time{std::to_string(minMinutes) + '-'
                           + std::to_string(maxMinutes) + " min left"};
    const std::string right{progress + " · " + time + " ─"};
    std::string separator{};
    const int contentCols{::getVisualLen(::utf8ToWide(left + ' ' + right))};
    for (int col{contentCols}; col < cols; ++col) {
      separator += "\u2500";
    }
    return esc + resetFG + "─ " + esc + yellowFG + "Test Title" + esc + resetFG
           + ' ' + separator + ' ' + esc + greenFG + progress + esc + resetFG
           + " · " + esc + blueFG + time + esc + resetFG + " ─";
  };

  assert(
      ::getChapterProgressIndicator(1, 10, 100, 10, englishStats, "Test Title")
      == expected(100, 4, 6, 100));
  assert(
      ::getChapterProgressIndicator(1, 10, 100, 110, englishStats, "Test Title")
      == expected(0, 4, 6, 100));
  assert(::getChapterProgressIndicator(10, 10, 100, 110, englishStats,
                                       "Test Title")
         == expected(9, 3, 6, 100));
  assert(::getChapterProgressIndicator(96, 10, 100, 110, englishStats,
                                       "Test Title")
         == expected(95, 1, 1, 100));
  assert(::getChapterProgressIndicator(101, 10, 100, 110, englishStats,
                                       "Test Title")
         == expected(100, 1, 1, 100));
  assert(
      ::getChapterProgressIndicator(4, 10, 100, 210, englishStats, "Test Title")
      == expected(2, 4, 6, 100));
  assert(::getChapterProgressIndicator(-10, 10, 100, 110, englishStats,
                                       "Test Title")
         == expected(0, 4, 6, 100));
  assert(::getChapterProgressIndicator(200, 10, 100, 110, englishStats,
                                       "Test Title")
         == expected(100, 0, 0, 100));
  assert(
      ::getChapterProgressIndicator(1, 10, 5, 110, englishStats, "Test Title")
      == esc + resetFG + "─ " + esc + blueFG + 't' + esc + resetFG + " ─");
  const std::string noTitleStatus{::getChapterProgressIndicator(
      1, 10, 34, 110, englishStats, "Test Title")};
  assert(::getVisualLen(::utf8ToWide(noTitleStatus)) == 34);
  assert(noTitleStatus.starts_with(esc + resetFG + "─ " + esc + greenFG));
  assert(noTitleStatus.ends_with(" ─"));
  const std::string truncatedTitleStatus{::getChapterProgressIndicator(
      1, 10, 43, 110, englishStats, "Test Title")};
  assert(::getVisualLen(::utf8ToWide(truncatedTitleStatus)) == 43);
  assert(truncatedTitleStatus.contains(esc + yellowFG + "…" + esc + resetFG));

  const std::string searchStatus{::getChapterProgressIndicator(
      1, 10, 100, 110, englishStats, "Test Title", "needle", "[2/4]")};
  assert(searchStatus.contains(esc + yellowFG + "/needle" + esc + resetFG
                               + " · " + esc + redFG + "[2/4]" + esc
                               + resetFG));
  assert(searchStatus.contains(esc + greenFG + "0% chapter progress"));
  const std::string pendingSearchStatus{::getChapterProgressIndicator(
      1, 10, 140, 110, englishStats, "Test Title", "needle", "3 matches")};
  assert(
      pendingSearchStatus.contains(esc + redFG + "3 matches" + esc + resetFG));
  int narrowSearchCursorCol{};
  const std::string narrowSearchStatus{::getChapterProgressIndicator(
      1, 10, 34, 110, englishStats, "Test Title", "needle", "3 matches",
      &narrowSearchCursorCol)};
  assert(narrowSearchStatus.contains(esc + yellowFG + "/ne…"));
  assert(narrowSearchStatus.contains(esc + redFG + "3 matches"));
  assert(::getVisualLen(::utf8ToWide(narrowSearchStatus)) == 34);
  assert(narrowSearchCursorCol == 7);

  const ChapterReadingStats mixedStats{.words = 320, .cjkCharacters = 700};
  assert(
      ::getChapterProgressIndicator(1, 10, 100, 110, mixedStats, "Test Title")
      == expected(0, 2, 5, 100));

  assert(::calcBotLineFromTopLine(1, 9) == 9);
  assert(::calcTopLineFromBotLine(110, 9) == 102);
}

auto chapterSearch() -> void {
  const std::string chapter{"  Hello " + esc + bold + "World" + esc + resetBold
                            + "\n  next line\n\n  ---\n"};
  const ChapterSearchIndex index{::buildChapterSearchIndex(chapter)};
  assert(index.text == "hello world next line");
  assert(index.source.size() == index.text.size());

  const std::vector<ChapterSearchMatch> caseInsensitive{
      ::findChapterSearchMatches(index, "HELLO WORLD")};
  assert(caseInsensitive.size() == 1);
  assert(caseInsensitive.front().line == 1);
  assert(::findChapterSearchMatches(index, "world next").size() == 1);
  assert(::findChapterSearchMatches(index, "missing").empty());
  assert(::findChapterSearchMatches(index, "  \n ").empty());
  assert(::findChapterSearchMatches(index, "---").empty());

  const std::string forcedWrapChapter{"  supercali" + forcedWrapMarker
                                      + "\n  fragilistic\n---\n"};
  const ChapterSearchIndex forcedWrapIndex{
      ::buildChapterSearchIndex(forcedWrapChapter)};
  assert(forcedWrapIndex.text == "supercalifragilistic");
  assert(
      ::findChapterSearchMatches(forcedWrapIndex, "supercalifragilistic").size()
      == 1);

  const std::string highlighted{::highlightSearchMatches(
      chapter, index, caseInsensitive, 0, chapter.size())};
  assert(highlighted.contains(esc + grayBG + "Hello"));
  assert(highlighted.contains("World" + esc + resetBG));
  const std::optional cursorPos{::getSearchMatchCursorPosition(
      chapter, index, caseInsensitive.front(), 1, 2)};
  assert((cursorPos == std::pair{1, 3}));

  const std::string wideCursorChapter{"  世" + esc + bold + "界 target" + esc
                                      + resetBold + "\n---\n"};
  const ChapterSearchIndex wideCursorIndex{
      ::buildChapterSearchIndex(wideCursorChapter)};
  const std::vector<ChapterSearchMatch> wideCursorMatches{
      ::findChapterSearchMatches(wideCursorIndex, "target")};
  assert(wideCursorMatches.size() == 1);
  assert((::getSearchMatchCursorPosition(wideCursorChapter, wideCursorIndex,
                                         wideCursorMatches.front(), 1, 1)
          == std::pair{1, 8}));
  assert(!::getSearchMatchCursorPosition(wideCursorChapter, wideCursorIndex,
                                         wideCursorMatches.front(), 2, 2));

  const std::vector<ChapterSearchMatch> multiLine{
      ::findChapterSearchMatches(index, "world next")};
  const std::string multiLineHighlighted{
      ::highlightSearchMatches(chapter, index, multiLine, 0, chapter.size())};
  assert(::getOccurrences<std::string_view>(multiLineHighlighted, esc + grayBG)
         == 2);
  assert(::getOccurrences<std::string_view>(multiLineHighlighted, esc + resetBG)
         == 2);

  const std::string repeated{"Banana banana\n---\n"};
  const ChapterSearchIndex repeatedIndex{::buildChapterSearchIndex(repeated)};
  const std::vector<ChapterSearchMatch> repeatedMatches{
      ::findChapterSearchMatches(repeatedIndex, "ana")};
  assert(repeatedMatches.size() == 2);
  const std::string allRepeatedHighlighted{::highlightSearchMatches(
      repeated, repeatedIndex, repeatedMatches, 0, repeated.size())};
  assert(
      ::getOccurrences<std::string_view>(allRepeatedHighlighted, esc + grayBG)
      == 2);
  const std::string unicode{"CAFÉ café\n---\n"};
  const ChapterSearchIndex unicodeIndex{::buildChapterSearchIndex(unicode)};
  assert(::findChapterSearchMatches(unicodeIndex, "CAFÉ").size() == 1);
  assert(::findChapterSearchMatches(unicodeIndex, "café").size() == 1);

  const std::string punctuation{"'single' \"double\" plain-word\n"
                                "‘single’ “double” plain‑word — dash\n---\n"};
  const ChapterSearchIndex punctuationIndex{
      ::buildChapterSearchIndex(punctuation)};
  assert(::findChapterSearchMatches(punctuationIndex, "'single'").size() == 2);
  assert(::findChapterSearchMatches(punctuationIndex, "‘single’").size() == 2);
  assert(::findChapterSearchMatches(punctuationIndex, "\"double\"").size()
         == 2);
  assert(::findChapterSearchMatches(punctuationIndex, "plain-word").size()
         == 2);
  assert(::findChapterSearchMatches(punctuationIndex, "- dash").size() == 1);

  const std::string nonBreakingSpaces{"first second narrow space\n---\n"};
  const ChapterSearchIndex nonBreakingSpaceIndex{
      ::buildChapterSearchIndex(nonBreakingSpaces)};
  assert(
      ::findChapterSearchMatches(nonBreakingSpaceIndex, "first second").size()
      == 1);
  assert(
      ::findChapterSearchMatches(nonBreakingSpaceIndex, "narrow space").size()
      == 1);

  const std::string imageChapter{"before\n" + esc + greenFG + imgCellPlaceholder
                                 + esc + resetFG + "\nafter\n---\n"};
  const ChapterSearchIndex imageIndex{::buildChapterSearchIndex(imageChapter)};
  assert(!imageIndex.text.contains(imgCellPlaceholder));
  assert(::findChapterSearchMatches(imageIndex, "before after").size() == 1);

  std::string utf8Query{"café"};
  ::popLastUTF8CodePoint(utf8Query);
  assert(utf8Query == "caf");
  ::popLastUTF8CodePoint(utf8Query);
  assert(utf8Query == "ca");
  std::string empty{};
  ::popLastUTF8CodePoint(empty);
  assert(empty.empty());

  std::string words{"find café noir  "};
  ::popLastSearchWord(words);
  assert(words == "find café ");
  ::popLastSearchWord(words);
  assert(words == "find ");
  ::popLastSearchWord(words);
  assert(words.empty());
  std::string nonBreakingWords{"first second"};
  ::popLastSearchWord(nonBreakingWords);
  assert(nonBreakingWords == "first ");
  ::popLastSearchWord(words);
  assert(words.empty());

  std::string inputQuery{};
  std::string pendingInput{};
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0xC3U));
  assert(inputQuery.empty());
  assert(::appendSearchInputByte(inputQuery, pendingInput, 0xA9U));
  assert(inputQuery == "é");
  assert(pendingInput.empty());
  assert(!::appendSearchInputByte(inputQuery, pendingInput, '\t'));
  assert(!::appendSearchInputByte(inputQuery, pendingInput, ctrlF));
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0xFFU));
  assert(inputQuery == "é");
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0xE0U));
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0x80U));
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0x80U));
  assert(inputQuery == "é");
  assert(pendingInput.empty());
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0xC2U));
  assert(!::appendSearchInputByte(inputQuery, pendingInput, 0x9BU));
  assert(inputQuery == "é");
  assert(pendingInput.empty());
}

auto displayChapter() -> void {
  ::enableRawMode();

  std::cout << esc << clearScreen;
  const std::pair imgChapterOutput{::displayChapter(
      imageElementsRootAbs / "Text/html-image.xhtml", "Test Title", 0, 55)};
  const std::pair textChapterOutput{::displayChapter(
      imageElementsRootAbs / "Text/prose.xhtml", "Test Title", 0.5, 55)};
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

auto tocDataToString() -> void {
  const TocData toc{{"Chapter 1", "chapter-1.xhtml"},
                    {"    Section 1", "section-1.xhtml"}};
  std::string str{};
  ::tocDataToString(toc, str);

  const std::string expected{"Chapter 1\n\n    Section 1\n" + centerAlignBegin
                             + esc + redFG + esc + bold + "---" + esc + resetFG
                             + esc + resetBold + centerAlignEnd + '\n'};
  assert(str == expected);

  const std::string tocStatus{::getTOCStatusLine(50, "Test Title")};
  const std::string expectedStatus{
      esc + resetFG + "─ " + esc + yellowFG + "Test Title" + esc + resetFG
      + " ───────────────── " + esc + magentaFG + "Table of Contents" + esc
      + resetFG + " ─"};
  assert(tocStatus == expectedStatus);
  const std::string narrowTOCStatus{::getTOCStatusLine(22, "Test Title")};
  assert(::getVisualLen(::utf8ToWide(narrowTOCStatus)) == 22);
  assert(narrowTOCStatus.starts_with(esc + resetFG + "─ " + esc + magentaFG));
  assert(narrowTOCStatus.ends_with(" ──"));
  const std::string truncatedTitleTOCStatus{
      ::getTOCStatusLine(26, "Test Title")};
  assert(::getVisualLen(::utf8ToWide(truncatedTitleTOCStatus)) == 26);
  assert(
      truncatedTitleTOCStatus.contains(esc + yellowFG + "…" + esc + resetFG));
}

auto displayTOC() -> void {
  ::enableRawMode();

  std::cout << esc << clearScreen;
  const fs::path metadataPathsTOCOutput{
      ::displayTOC(::getTOC(metadataPathsRootAbs / "toc.ncx"),
                   "The Clockwork Garden", 55, 1)};
  const fs::path nonlinearSpineTOCOutput{
      ::displayTOC(::getTOC(nonlinearSpineRootAbs / "OEBPS/toc.ncx"),
                   "Rooms Within Rooms", 55, 0)};
  eraseScreen();

  boldColorIfTerm(stdout, yellowFG);
  std::cout << "`test::displayTOC()` success cases need verification, "
               "a tui interface for the metadata/path and nested "
               "package/spine TOCs "
               "should have been displayed.\n";
  resetBoldColorIfTerm(stdout);

  std::cout << "metadata paths output: " << metadataPathsTOCOutput << '\n';
  std::cout << "nonlinear spine output: " << nonlinearSpineTOCOutput << '\n';
}

auto displayEpub() -> void {
  ::enableRawMode();

  const EpubProg metadataPathsIniProg{
      .chapterAbs = metadataPathsRootAbs / "chapters/chapter one.xhtml",
      .chapterProg = 0.5};
  const EpubProg nonlinearSpineIniProg{.chapterAbs = "", .chapterProg = 0};
  const EpubProg imageElementsIniProg{.chapterAbs = imageElementsRootAbs
                                                    / "Text/prose.xhtml",
                                      .chapterProg = 0.25};
  const EpubProg nestedNavigationIniProg{.chapterAbs = nestedNavigationRootAbs
                                                       / "Book/Text/two.xhtml",
                                         .chapterProg = 0.75};

  const EpubProg metadataPathsOut{
      ::displayEpub(metadataPathsIniProg, metadataPathsRootAbs, 50)};
  const EpubProg nonlinearSpineOut{
      ::displayEpub(nonlinearSpineIniProg, nonlinearSpineRootAbs, 55)};
  const EpubProg imageElementsOut{
      ::displayEpub(imageElementsIniProg, imageElementsRootAbs, 60)};
  const EpubProg nestedNavigationOut{
      ::displayEpub(nestedNavigationIniProg, nestedNavigationRootAbs, 65)};

  boldColorIfTerm(stdout, yellowFG);
  std::cout << "`test::displayEpub()` success cases need verification, "
               "a tui interface for all four "
               "test epubs should have been displayed.\n";
  resetBoldColorIfTerm(stdout);

  std::cout << "metadata paths exit chapter path: ";
  std::cout << metadataPathsOut.chapterAbs << '\n';
  std::cout << "metadata paths exit chapter prog: ";
  std::cout << metadataPathsOut.chapterProg << '\n';
  std::cout << "nonlinear spine exit chapter path: ";
  std::cout << nonlinearSpineOut.chapterAbs << '\n';
  std::cout << "nonlinear spine exit chapter prog: ";
  std::cout << nonlinearSpineOut.chapterProg << '\n';
  std::cout << "image elements exit chapter path: ";
  std::cout << imageElementsOut.chapterAbs << '\n';
  std::cout << "image elements exit chapter prog: ";
  std::cout << imageElementsOut.chapterProg << '\n';
  std::cout << "nested navigation exit chapter path: ";
  std::cout << nestedNavigationOut.chapterAbs << '\n';
  std::cout << "nested navigation exit chapter prog: ";
  std::cout << nestedNavigationOut.chapterProg << '\n';
}

auto styleEachLineIndividually() -> void {
  std::string str{"\033[1mfirst line\nsec line\033[22mout"};
  ::styleEachLineIndividually(str, "\033[1m", "\033[22m");
  assert(str == "\033[1mfirst line\033[22m\n\033[1msec line\033[22mout");

  const std::array<std::string_view, 14> styleCodes{
      bold,           resetBold, italic,      resetItalic, underline,
      resetUnderline, yellowFG,  redFG,       greenFG,     blueFG,
      magentaFG,      cyanFG,    lightGrayFG, resetFG};
  for (const std::string_view styleCode : styleCodes) {
    const std::wstring escapedStyle{::utf8ToWide(esc + std::string{styleCode})};
    assert(::getVisualLen(escapedStyle) == 0);
  }

  std::string underlined{esc + underline + "one two three" + esc
                         + resetUnderline};
  ::processContentText(underlined, 7);
  assert(getOccurrences<std::string_view>(underlined, esc + underline) == 2);
  assert(getOccurrences<std::string_view>(underlined, esc + resetUnderline)
         == 2);
  assert(underlined.contains(esc + underline + "one two" + esc + resetUnderline
                             + '\n' + esc + underline));

  std::string centeredUnderlined{centerAlignBegin + esc + underline
                                 + "one two three" + esc + resetUnderline
                                 + centerAlignEnd};
  ::processContentText(centeredUnderlined, 7);
  const std::size_t newline{centeredUnderlined.find('\n')};
  assert(newline != std::string::npos);
  const std::size_t secondUnderline{
      centeredUnderlined.find(esc + underline, newline + 1)};
  assert(secondUnderline != std::string::npos);
  assert(secondUnderline > newline + 1);
  assert(centeredUnderlined.find_first_not_of(' ', newline + 1)
         == secondUnderline);
}

auto headingColors() -> void {
  XMLDocument chapter{};
  assert(chapter.Parse("<body>"
                       "<h1><strong><em><code>H1</code></em></strong></h1>"
                       "<h2><strong><em><code>H2</code></em></strong></h2>"
                       "<h3><strong><em><code>H3</code></em></strong></h3>"
                       "<h4><strong><em><code>H4</code></em></strong></h4>"
                       "<h5><strong><em><code>H5</code></em></strong></h5>"
                       "<h6><strong><em><code>H6</code></em></strong></h6>"
                       "</body>")
         == XML_SUCCESS);

  std::string parsed{};
  ::parseContentElem(chapter.FirstChildElement("body"), parsed, {});
  const std::array<std::string_view, 6> colors{magentaFG, magentaFG, magentaFG,
                                               magentaFG, magentaFG, magentaFG};
  const std::array<bool, 6> boldLevels{true, true, false, false, false, false};
  const std::array<bool, 6> italicLevels{false, false, true,
                                         true,  false, false};
  const std::array<bool, 6> underlinedLevels{true,  false, true,
                                             false, false, false};
  for (std::size_t level{0}; level < colors.size(); ++level) {
    std::string expected{centerAlignBegin};
    if (boldLevels.at(level)) {
      expected += esc + bold;
    }
    if (italicLevels.at(level)) {
      expected += esc + italic;
    }
    if (underlinedLevels.at(level)) {
      expected += esc + underline;
    }
    expected += esc;
    expected += colors.at(level);
    expected += 'H';
    expected += std::to_string(level + 1);
    if (boldLevels.at(level)) {
      expected += esc + resetBold;
    }
    if (italicLevels.at(level)) {
      expected += esc + resetItalic;
    }
    if (underlinedLevels.at(level)) {
      expected += esc + resetUnderline;
    }
    expected += esc;
    expected += resetFG;
    expected += centerAlignEnd;
    assert(parsed.contains(expected));
  }

  XMLDocument nestedHeading{};
  assert(nestedHeading.Parse(
             "<body><h2>before <span><code>code</code></span> after "
             "<strong>bold</strong></h2></body>")
         == XML_SUCCESS);
  std::string nestedHeadingParsed{};
  ::parseContentElem(nestedHeading.FirstChildElement("body"),
                     nestedHeadingParsed, {});
  const std::string expectedNestedHeading{
      "\n\n" + centerAlignBegin + esc + bold + esc + magentaFG
      + "before code after bold" + esc + resetBold + esc + resetFG
      + centerAlignEnd + "\n\n"};
  assert(nestedHeadingParsed == expectedNestedHeading);

  XMLDocument nestedStyles{};
  assert(nestedStyles.Parse("<body><p><em>outer <i>inner</i> after</em></p>"
                            "<p><code>outer <code>inner</code> after</code></p>"
                            "<strong>before<hr/>after</strong></body>")
         == XML_SUCCESS);
  std::string nestedStylesParsed{};
  ::parseContentElem(nestedStyles.FirstChildElement("body"), nestedStylesParsed,
                     {});
  assert(getOccurrences<std::string_view>(nestedStylesParsed, esc + italic)
         == 1);
  assert(getOccurrences<std::string_view>(nestedStylesParsed, esc + resetItalic)
         == 1);
  assert(getOccurrences<std::string_view>(nestedStylesParsed, esc + greenFG)
         == 1);
  assert(getOccurrences<std::string_view>(nestedStylesParsed, esc + resetFG)
         == 1);
  assert(getOccurrences<std::string_view>(nestedStylesParsed, esc + bold) == 1);
  assert(getOccurrences<std::string_view>(nestedStylesParsed, esc + resetBold)
         == 1);

  std::string processed{centerAlignBegin + esc + lightGrayFG + "one two three"
                        + esc + resetFG + centerAlignEnd};
  ::processContentText(processed, 7);
  assert(!processed.contains(centerAlignBegin));
  assert(!processed.contains(centerAlignEnd));
  assert(getOccurrences<std::string_view>(processed, esc + lightGrayFG) == 2);
  const std::size_t newline{processed.find('\n')};
  assert(newline != std::string::npos);
  assert(
      processed.contains(esc + lightGrayFG + "one two" + esc + resetFG + '\n'));
  const std::size_t secondLightGray{processed.rfind(esc + lightGrayFG)};
  assert(secondLightGray > newline + 1);
  assert(processed.find_first_not_of(' ', newline + 1) == secondLightGray);
  const std::size_t three{processed.find("three", secondLightGray)};
  assert(three == secondLightGray + esc.size() + lightGrayFG.size());

  ::processContentText(nestedHeadingParsed, 10);
  assert(getOccurrences<std::string_view>(nestedHeadingParsed, esc + greenFG)
         == 0);
  assert(getOccurrences<std::string_view>(nestedHeadingParsed, esc + bold)
         == 3);
}

auto contentAlignment() -> void {
  XMLDocument chapter{};
  assert(
      chapter.Parse("<body><p class='centerp section-marking'>center</p>"
                    "<p class='separator'>separator</p>"
                    "<div class='ornamental-break'><p "
                    "class='ornamental-break-as-text'>fallback</p></div>"
                    "<p class='right'>right</p>"
                    "<div class='center'><p>inherited</p>"
                    "<p class='justify'>justified</p>"
                    "<p style='color: red; text-align : right !important'>"
                    "inline</p></div>"
                    "<p class='right'>before <code>code</code> after</p>"
                    "<div class='center'><pre>pre</pre></div>"
                    "<div class='right'><table><tr><td>one</td><td>two</td>"
                    "</tr></table></div>"
                    "<div class='right'><hr/></div>"
                    "<p "
                    "align='center'>legacy</p><center><p>element</p></center>"
                    "<center>direct</center>"
                    "<div style='text-align: right'>direct right</div>"
                    "<p align='right'>legacy right</p>"
                    "<p class='centerpiece'>centerpiece</p>"
                    "<p class='section-break'>section</p>"
                    "<p class='space-break'>space</p>"
                    "<p class='signature'>signature</p><p>***</p>"
                    "<h2 style='text-align: right'>heading</h2></body>")
      == XML_SUCCESS);

  std::string parsed{};
  ::parseContentElem(chapter.FirstChildElement("body"), parsed, {});

  std::string expected{};
  const auto appendBlockBoundary = [&expected]() -> void {
    expected += "\n\n";
  };
  const auto appendParagraph = [&expected](std::string_view begin,
                                           std::string_view text,
                                           std::string_view end) -> void {
    expected += "\n\n";
    expected += begin;
    expected += text;
    expected += end;
    expected += "\n\n";
  };
  const std::string tableSeparator{esc + cyanFG + " | " + esc + resetFG};
  appendParagraph(centerAlignBegin, "center", centerAlignEnd);
  appendParagraph(centerAlignBegin, "separator", centerAlignEnd);
  appendBlockBoundary();
  appendParagraph(centerAlignBegin, "fallback", centerAlignEnd);
  appendBlockBoundary();
  appendParagraph(rightAlignBegin, "right", rightAlignEnd);
  appendBlockBoundary();
  appendParagraph(centerAlignBegin, "inherited", centerAlignEnd);
  appendParagraph("", "justified", "");
  appendParagraph(rightAlignBegin, "inline", rightAlignEnd);
  appendBlockBoundary();
  appendParagraph(rightAlignBegin,
                  "before " + esc + greenFG + "code" + esc + resetFG + " after",
                  rightAlignEnd);
  appendBlockBoundary();
  appendParagraph(centerAlignBegin + esc + greenFG, "pre",
                  esc + resetFG + centerAlignEnd);
  appendBlockBoundary();
  appendBlockBoundary();
  appendBlockBoundary();
  expected += rightAlignBegin;
  expected += '\n';
  expected += "one" + tableSeparator + "two";
  expected += rightAlignEnd;
  appendBlockBoundary();
  appendBlockBoundary();
  appendBlockBoundary();
  appendBlockBoundary();
  expected += centerAlignBegin;
  expected += esc;
  expected += bold;
  expected += "***";
  expected += esc;
  expected += resetBold;
  expected += centerAlignEnd;
  appendBlockBoundary();
  appendBlockBoundary();
  appendParagraph(centerAlignBegin, "legacy", centerAlignEnd);
  appendBlockBoundary();
  appendParagraph(centerAlignBegin, "element", centerAlignEnd);
  appendBlockBoundary();
  appendParagraph(centerAlignBegin, "direct", centerAlignEnd);
  appendParagraph(rightAlignBegin, "direct right", rightAlignEnd);
  appendParagraph(rightAlignBegin, "legacy right", rightAlignEnd);
  appendParagraph("", "centerpiece", "");
  appendParagraph("", "section", "");
  appendParagraph("", "space", "");
  appendParagraph("", "signature", "");
  appendParagraph("", "***", "");
  appendBlockBoundary();
  expected += centerAlignBegin;
  expected += esc;
  expected += bold;
  expected += esc;
  expected += magentaFG;
  expected += "heading";
  expected += esc;
  expected += resetBold;
  expected += esc;
  expected += resetFG;
  expected += centerAlignEnd;
  expected += "\n\n";
  assert(parsed == expected);

  XMLDocument trailingWhitespacePre{};
  assert(trailingWhitespacePre.Parse("<body>before<pre>pre\n</pre>after</body>")
         == XML_SUCCESS);
  std::string trailingWhitespacePreParsed{};
  ::parseContentElem(trailingWhitespacePre.FirstChildElement("body"),
                     trailingWhitespacePreParsed, {});
  assert(trailingWhitespacePreParsed
         == "before\n\n" + esc + greenFG + "pre" + esc + resetFG
                + "\n\n\nafter");

  ::processContentText(trailingWhitespacePreParsed, 55);
  assert(getOccurrences<std::string_view>(trailingWhitespacePreParsed, "\n")
         == 4);

  XMLDocument tableChapter{};
  assert(tableChapter.Parse(
             "<body>before<table><caption>caption</caption>"
             "<colgroup><col/></colgroup><thead><tr><th>head one</th>"
             "<th>head two</th></tr></thead><tbody><tr><td>body one</td>"
             "<td></td><td>body two</td><td/></tr></tbody><tfoot><tr>"
             "<td>foot one</td><td>foot two</td></tr></tfoot></table>"
             "after</body>")
         == XML_SUCCESS);
  std::string tableParsed{};
  ::parseContentElem(tableChapter.FirstChildElement("body"), tableParsed, {});
  assert(tableParsed.starts_with("before\n\n\n\ncaption\n\n\n"));
  assert(tableParsed.ends_with("foot one" + tableSeparator + "foot two\n\n"
                               + "after"));
  assert(tableParsed.contains(esc + bold + "head one" + esc + resetBold
                              + tableSeparator + esc + bold + "head two" + esc
                              + resetBold + '\n'));
  assert(tableParsed.contains("body one" + tableSeparator + "body two\nfoot "
                              + "one" + tableSeparator + "foot two"));
  assert(getOccurrences<std::string_view>(tableParsed, tableSeparator) == 3);
  assert(!tableParsed.contains(tableSeparator + tableSeparator));

  XMLDocument tableBlockChapter{};
  assert(tableBlockChapter.Parse(
             "<body><table><tr><td><p>one</p></td>"
             "<td><div><p>two</p></div></td></tr></table></body>")
         == XML_SUCCESS);
  std::string tableBlockParsed{};
  ::parseContentElem(tableBlockChapter.FirstChildElement("body"),
                     tableBlockParsed, {});
  assert(tableBlockParsed == "\n\n\none" + tableSeparator + "two\n\n");

  std::string tableProcessed{tableParsed};
  ::processContentText(tableProcessed, 55);
  assert(!tableProcessed.contains("\n\n\n"));
  const auto assertLineBoundary = [&tableProcessed](std::string_view before,
                                                    std::string_view after,
                                                    int newlineCount) -> void {
    const std::size_t beforeBegin{tableProcessed.find(before)};
    assert(beforeBegin != std::string_view::npos);
    const std::size_t beforeEnd{beforeBegin + before.size()};
    const std::size_t afterBegin{tableProcessed.find(after, beforeEnd)};
    assert(afterBegin != std::string_view::npos);
    const std::string_view boundary{tableProcessed.data() + beforeEnd,
                                    afterBegin - beforeEnd};
    assert(boundary.starts_with('\n'));
    assert(getOccurrences<std::string_view>(boundary, "\n") == newlineCount);
    assert(boundary.find_first_not_of("\n ") == std::string_view::npos);
  };
  assertLineBoundary("body two", "foot one", 1);
  assertLineBoundary("caption", esc + bold + "head one", 2);

  XMLDocument styledTable{};
  assert(styledTable.Parse(
             "<body><strong><em><table class='right'>"
             "<caption align='center'>caption</caption>"
             "<tr><td>one</td><td>two</td></tr></table></em></strong>"
             "</body>")
         == XML_SUCCESS);
  std::string styledTableParsed{};
  ::parseContentElem(styledTable.FirstChildElement("body"), styledTableParsed,
                     {});
  const std::string unstyledSeparator{esc + resetBold + esc + resetItalic + esc
                                      + cyanFG + " | " + esc + bold + esc
                                      + italic + esc + resetFG};
  assert(styledTableParsed.contains("one" + unstyledSeparator + "two"));
  assert(getOccurrences<std::string_view>(styledTableParsed, rightAlignBegin)
         == 1);
  assert(getOccurrences<std::string_view>(styledTableParsed, rightAlignEnd)
         == 1);
  assert(!styledTableParsed.contains(centerAlignBegin));

  std::string styledTableProcessed{styledTableParsed};
  ::processContentText(styledTableProcessed, 55);
  assert(!styledTableProcessed.contains(rightAlignBegin));
  assert(!styledTableProcessed.contains(rightAlignEnd));
  assert(getOccurrences<std::string_view>(styledTableProcessed, "\n") == 6);

  XMLDocument styledRule{};
  assert(styledRule.Parse("<body><em><code><hr/></code></em></body>")
         == XML_SUCCESS);
  std::string styledRuleParsed{};
  ::parseContentElem(styledRule.FirstChildElement("body"), styledRuleParsed,
                     {});
  assert(styledRuleParsed.contains(
      centerAlignBegin + esc + bold + esc + resetItalic + esc + resetFG + "***"
      + esc + resetBold + esc + italic + esc + greenFG + centerAlignEnd));

  XMLDocument nestedChapter{};
  assert(
      nestedChapter.Parse("<body><li class='right'><h2>Nested heading</h2></li>"
                          "</body>")
      == XML_SUCCESS);
  std::string nestedParsed{};
  ::parseContentElem(nestedChapter.FirstChildElement("body"), nestedParsed, {});
  assert(!nestedParsed.contains(rightAlignBegin));
  assert(nestedParsed.contains(centerAlignBegin + esc + bold + esc + magentaFG
                               + "Nested heading"));

  XMLDocument blockChapter{};
  assert(blockChapter.Parse("<body>before<div><p>inside</p></div>after</body>")
         == XML_SUCCESS);
  std::string blockParsed{};
  ::parseContentElem(blockChapter.FirstChildElement("body"), blockParsed, {});
  assert(blockParsed == "before\n\n\n\ninside\n\n\n\nafter");

  constexpr std::array<std::string_view, 8> semanticBlocks{
      "section", "article", "aside",      "main",
      "header",  "footer",  "blockquote", "center"};
  for (const std::string_view name : semanticBlocks) {
    XMLDocument semanticBlockChapter{};
    const std::string source{"<body>before<" + std::string{name}
                             + "><p>inside</p></" + std::string{name}
                             + ">after</body>"};
    assert(semanticBlockChapter.Parse(source.c_str()) == XML_SUCCESS);
    std::string semanticBlockParsed{};
    ::parseContentElem(semanticBlockChapter.FirstChildElement("body"),
                       semanticBlockParsed, {});
    assert(semanticBlockParsed.starts_with("before\n\n\n\n"));
    assert(semanticBlockParsed.ends_with("\n\n\n\nafter"));
  }

  XMLDocument missingImageChapter{};
  assert(missingImageChapter.Parse(
             "<body>before<img src='missing.png'/>after</body>")
         == XML_SUCCESS);
  std::string missingImageParsed{};
  ::parseContentElem(missingImageChapter.FirstChildElement("body"),
                     missingImageParsed, {});
  assert(missingImageParsed == "beforeafter");

  std::string centered{centerAlignBegin + "one two\nthree" + centerAlignEnd};
  ::centerJustify(centerAlignBegin, centerAlignEnd, centered, 7);
  assert(centered == centerAlignBegin + "one two\n three" + centerAlignEnd);

  std::string rightAligned{rightAlignBegin + "one two\nthree" + rightAlignEnd};
  ::rightJustify(rightAlignBegin, rightAlignEnd, rightAligned, 7);
  assert(rightAligned == rightAlignBegin + "one two\n  three" + rightAlignEnd);

  std::string overlong{centerAlignBegin + "toolong" + centerAlignEnd};
  ::centerJustify(centerAlignBegin, centerAlignEnd, overlong, 3);
  assert(overlong == centerAlignBegin + "toolong" + centerAlignEnd);

  std::string processed{centerAlignBegin + "center" + centerAlignEnd + '\n'
                        + rightAlignBegin + "right" + rightAlignEnd + '\n' + esc
                        + yellowFG + "color" + esc + resetFG + '\n'
                        + centerAlignBegin + "dangling"};
  ::processContentText(processed, 9);
  assert(!processed.contains(centerAlignBegin));
  assert(!processed.contains(centerAlignEnd));
  assert(!processed.contains(rightAlignBegin));
  assert(!processed.contains(rightAlignEnd));
  assert(getOccurrences<std::string_view>(processed, esc + yellowFG) == 1);

  const std::string_view processedView{processed};
  const std::size_t firstNewline{processedView.find('\n')};
  const std::size_t secondNewline{processedView.find('\n', firstNewline + 1)};
  const std::size_t thirdNewline{processedView.find('\n', secondNewline + 1)};
  const std::string_view centerLine{processedView.substr(0, firstNewline)};
  const std::string_view rightLine{
      processedView.substr(firstNewline + 1, secondNewline - firstNewline - 1)};
  const std::string_view colorLine{processedView.substr(
      secondNewline + 1, thirdNewline - secondNewline - 1)};
  const auto leadingSpaces = [](std::string_view line) -> std::size_t {
    return line.find_first_not_of(' ');
  };
  assert(leadingSpaces(centerLine) == leadingSpaces(colorLine) + 1);
  assert(leadingSpaces(rightLine) == leadingSpaces(colorLine) + 4);

  std::string fixture{};
  ::parseChapter(nonlinearSpineRootAbs / "OEBPS/Text/intro.xhtml", fixture);
  assert(fixture.contains(centerAlignBegin + "- * -" + centerAlignEnd));
  std::string terminator{centerAlignBegin};
  terminator += esc;
  terminator += bold;
  terminator += esc;
  terminator += redFG;
  terminator += "---";
  terminator += esc;
  terminator += resetBold;
  terminator += esc;
  terminator += resetFG;
  terminator += centerAlignEnd;
  terminator += '\n';
  assert(fixture.ends_with(terminator));
}

auto initConf() -> void {
  const fs::path tmpConfigAbs{fs::temp_directory_path()
                              / "epubworm_test_initConf"};
  fs::remove_all(tmpConfigAbs);
  fs::create_directories(tmpConfigAbs);

  const fs::path configFileAbs{tmpConfigAbs / "epubworm/conf.xml"};
  ::initConf(configFileAbs);

  assert(fs::exists(configFileAbs));

  XMLDocument configDoc{};
  assert(configDoc.LoadFile(configFileAbs.c_str()) == XML_SUCCESS);
  assert(configDoc.FirstChildElement("conf") != nullptr);
  assert(configDoc.FirstChildElement("conf")->FirstChildElement() == nullptr);

  fs::remove_all(tmpConfigAbs);
}

auto initLibrary() -> void {
  const fs::path tmpShareAbs{fs::temp_directory_path()
                             / "epubworm_test_initLibrary"};
  fs::remove_all(tmpShareAbs);
  fs::create_directories(tmpShareAbs);

  const fs::path libraryFileAbs{tmpShareAbs / "epubworm/library.xml"};
  ::initLibrary(libraryFileAbs);

  assert(fs::exists(libraryFileAbs));

  XMLDocument libraryDoc{};
  assert(libraryDoc.LoadFile(libraryFileAbs.c_str()) == XML_SUCCESS);
  const XMLElement* const libraryRoot{libraryDoc.FirstChildElement("library")};
  assert(libraryRoot != nullptr);
  const XMLElement* const lastRead{libraryRoot->FirstChildElement("last-read")};
  assert(lastRead != nullptr);
  assert(lastRead->NextSiblingElement() == nullptr);
  const char* const id{lastRead->Attribute("id")};
  assert(id != nullptr);
  assert(std::string_view{id}.empty());

  fs::remove_all(tmpShareAbs);
}

auto readConfig() -> void {
  const fs::path tmpConfigAbs{fs::temp_directory_path()
                              / "epubworm_test_readConfig"};
  fs::remove_all(tmpConfigAbs);
  fs::create_directories(tmpConfigAbs);

  const fs::path configFileAbs{tmpConfigAbs / "epubworm/conf.xml"};
  ::initConf(configFileAbs);
  assert(::readConfig(configFileAbs).lineLength == 55);

  fs::remove_all(tmpConfigAbs);
}

auto getTruncatedSHA256Sum() -> void {

  assert(::getTruncatedSHA256Sum(epubsAbs / "metadata_paths.epub")
         == "ded2eb033d88f30e1f790619f514e305");
  assert(::getTruncatedSHA256Sum(epubsAbs / "nonlinear_spine.epub")
         == "fd87f609e8d91fc8902692871b8b763a");
  assert(::getTruncatedSHA256Sum(epubsAbs / "image_elements.epub")
         == "1dc96bb730ceeb1f80a7565542bf6d7e");
  assert(::getTruncatedSHA256Sum(epubsAbs / "nested_navigation.epub")
         == "18c3f848ec2b8ca63ff8d57c88f0269c");
}

auto findEpubById() -> void {
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
  assert(std::string_view{::findEpubById(libraryRoot, "aaaa1")->Attribute("id")}
         == idA);

  // Ambiguous prefix: matches both idA and idB -> nullptr.
  assert(::findEpubById(libraryRoot, "aaaa") == nullptr);

  // Empty prefix: matches all three -> nullptr.
  assert(::findEpubById(libraryRoot, "") == nullptr);

  // No-match prefix: nullptr.
  assert(::findEpubById(libraryRoot, "cccc") == nullptr);
}

auto getUnambiguousEpubIdPrefix() -> void {
  constexpr std::string_view idA{"01234567a11111111111111111111111"};
  constexpr std::string_view idB{"01234567b22222222222222222222222"};
  constexpr std::string_view idC{"fedcba98333333333333333333333333"};

  XMLDocument library{};
  library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<library>"
                "<epub id=\"01234567a11111111111111111111111\"/>"
                "<epub id=\"01234567b22222222222222222222222\"/>"
                "<epub id=\"fedcba98333333333333333333333333\"/>"
                "</library>");
  assert(!library.Error());

  const XMLElement* const libraryRoot{library.FirstChildElement("library")};
  assert(libraryRoot != nullptr);

  assert(::getUnambiguousEpubIdPrefix(libraryRoot, idA) == "01234567a");
  assert(::getUnambiguousEpubIdPrefix(libraryRoot, idB) == "01234567b");
  assert(::getUnambiguousEpubIdPrefix(libraryRoot, idC) == "fedc");
}

auto addToLibrary() -> void {
  const fs::path tmpShareAbs{fs::temp_directory_path()
                             / "epubworm_test_addToLibrary"};
  fs::remove_all(tmpShareAbs);
  fs::create_directories(tmpShareAbs);

  const fs::path libraryFileAbs{tmpShareAbs / "epubworm/library.xml"};
  ::initLibrary(libraryFileAbs);

  constexpr std::string_view previousLastRead{"existing-id"};
  XMLDocument initialLibrary{};
  assert(initialLibrary.LoadFile(libraryFileAbs.c_str()) == XML_SUCCESS);
  ::setLastRead(initialLibrary, previousLastRead);
  assert(initialLibrary.SaveFile(libraryFileAbs.c_str()) == XML_SUCCESS);

  const std::string metadataPathsId{"ded2eb033d88f30e1f790619f514e305"};
  const std::expected<EpubInfo, LibraryUpdateError> added{
      ::addToLibrary(epubsAbs / "metadata_paths.epub", tmpShareAbs)};
  assert(added.has_value());
  assert(added.value().id == metadataPathsId);
  assert(added.value().title == "The Clockwork Garden");
  assert(added.value().author == "Epubworm Project");

  XMLDocument libraryDoc{};
  assert(libraryDoc.LoadFile(libraryFileAbs.c_str()) == XML_SUCCESS);
  const XMLElement* const libraryRoot{libraryDoc.FirstChildElement("library")};
  assert(libraryRoot != nullptr);

  const XMLElement* const lastRead{libraryRoot->FirstChildElement("last-read")};
  assert(lastRead != nullptr);
  const char* const lastReadId{lastRead->Attribute("id")};
  assert(lastReadId != nullptr);
  assert(std::string_view{lastReadId} == previousLastRead);

  const XMLElement* const epub{libraryRoot->FirstChildElement("epub")};
  assert(epub != nullptr);
  assert(epub->NextSiblingElement("epub") == nullptr);
  assert(std::string_view{epub->Attribute("id")} == metadataPathsId);
  assert(std::string_view{epub->Attribute("opened-chapter")}.empty());
  assert(epub->DoubleAttribute("chapter-progress") == 0.0);

  assert(fs::is_directory(tmpShareAbs / "epubworm/extracted_epubs"
                          / metadataPathsId));

  const std::expected<EpubInfo, LibraryUpdateError> duplicate{
      ::addToLibrary(epubsAbs / "metadata_paths.epub", tmpShareAbs)};
  assert(!duplicate.has_value());
  assert(duplicate.error() == LibraryUpdateError::alreadyInLibrary);

  fs::remove_all(tmpShareAbs);
}

auto queryEpubElem() -> void {
  const fs::path phonyShareAbs{"/phony_share"};
  const std::string_view idA{"11111111111111111111111111111111"};
  const std::string_view idB{"22222222222222222222222222222222"};

  XMLDocument library{};
  library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<library>"
                "<last-read id=\"11111111111111111111111111111111\"/>"
                "<epub id=\"11111111111111111111111111111111\" "
                "opened-chapter=\"chapters/chapter one.xhtml\" "
                "chapter-progress=\"0.5\"/>"
                "<epub id=\"22222222222222222222222222222222\" "
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
         == phonyShareAbs / "epubworm/extracted_epubs" / idA
                / "chapters/chapter one.xhtml");
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
  badLibrary.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
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
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()} == "epub entry missing `id` attribute");
  }

  // Failure case: missing `chapter-progress` attribute.
  XMLDocument noProgLibrary{};
  noProgLibrary.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                      "<library>"
                      "<epub id=\"abc\" opened-chapter=\"x.html\"/>"
                      "</library>");
  assert(!noProgLibrary.Error());
  const XMLElement* const noProgEpub{
      noProgLibrary.FirstChildElement("library")->FirstChildElement("epub")};
  assert(noProgEpub != nullptr);

  try {
    (void)::queryEpubElem(noProgEpub, phonyShareAbs);
    assert(false);
  }
  catch (const std::runtime_error& e) {
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
      badProgLibrary.FirstChildElement("library")->FirstChildElement("epub")};
  assert(badProgEpub != nullptr);

  try {
    (void)::queryEpubElem(badProgEpub, phonyShareAbs);
    assert(false);
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()}
           == "epub entry has invalid `chapter-progress` attribute");
  }
}

auto writeProgress() -> void {
  const fs::path phonyShareAbs{"/phony_share"};
  const std::string_view id{"11111111111111111111111111111111"};

  XMLDocument library{};
  library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<library>"
                "<epub id=\"11111111111111111111111111111111\" "
                "opened-chapter=\"\" chapter-progress=\"0\"/>"
                "</library>");
  assert(!library.Error());
  XMLElement* const libraryRoot{library.FirstChildElement("library")};
  assert(libraryRoot != nullptr);
  XMLElement* const epub{libraryRoot->FirstChildElement("epub")};
  assert(epub != nullptr);

  // Success case A: non-empty chapterAbs, non-zero progress.
  const EpubProg prog{.chapterAbs = phonyShareAbs / "epubworm/extracted_epubs"
                                    / id / "chapters/chapter one.xhtml",
                      .chapterProg = 0.5};
  ::writeProgress(epub, prog, phonyShareAbs);
  assert(std::string_view{epub->Attribute("opened-chapter")}
         == "chapters/chapter one.xhtml");
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
  badLibrary.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                   "<library>"
                   "<epub opened-chapter=\"x.html\" chapter-progress=\"0.1\"/>"
                   "</library>");
  assert(!badLibrary.Error());
  const XMLElement* const badEpub{
      badLibrary.FirstChildElement("library")->FirstChildElement("epub")};
  assert(badEpub != nullptr);

  try {
    ::writeProgress(const_cast<XMLElement*>(badEpub), prog, phonyShareAbs);
    assert(false);
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()} == "epub entry missing `id` attribute");
  }
}

auto deleteFromLibrary() -> void {
  const fs::path tmpShareAbs{fs::temp_directory_path()
                             / "epubworm_test_deleteFromLibrary"};
  fs::remove_all(tmpShareAbs);
  fs::create_directories(tmpShareAbs);

  const fs::path libraryFileAbs{tmpShareAbs / "epubworm/library.xml"};
  ::initLibrary(libraryFileAbs);

  assert(::addToLibrary(epubsAbs / "metadata_paths.epub", tmpShareAbs)
             .has_value());

  const std::string metadataPathsID{"ded2eb033d88f30e1f790619f514e305"};
  XMLDocument libraryBeforeDelete{};
  assert(libraryBeforeDelete.LoadFile(libraryFileAbs.c_str()) == XML_SUCCESS);
  ::setLastRead(libraryBeforeDelete, metadataPathsID);
  assert(libraryBeforeDelete.SaveFile(libraryFileAbs.c_str()) == XML_SUCCESS);

  const std::expected<EpubInfo, LibraryUpdateError> removed{
      ::deleteFromLibrary(metadataPathsID, tmpShareAbs)};
  assert(removed.has_value());
  assert(removed.value().id == metadataPathsID);
  assert(removed.value().title == "The Clockwork Garden");
  assert(removed.value().author == "Epubworm Project");

  XMLDocument libraryDoc{};
  assert(libraryDoc.LoadFile(libraryFileAbs.c_str()) == XML_SUCCESS);
  const XMLElement* const libraryRoot{libraryDoc.FirstChildElement("library")};
  assert(libraryRoot != nullptr);
  assert(libraryRoot->FirstChildElement("epub") == nullptr);
  const XMLElement* const lastRead{libraryRoot->FirstChildElement("last-read")};
  assert(lastRead != nullptr);
  const char* const id{lastRead->Attribute("id")};
  assert(id != nullptr);
  assert(std::string_view{id}.empty());

  assert(
      !fs::exists(tmpShareAbs / "epubworm/extracted_epubs" / metadataPathsID));

  const std::expected<EpubInfo, LibraryUpdateError> alreadyRemoved{
      ::deleteFromLibrary(metadataPathsID, tmpShareAbs)};
  assert(!alreadyRemoved.has_value());
  assert(alreadyRemoved.error() == LibraryUpdateError::notFoundOrAmbiguous);

  fs::remove_all(tmpShareAbs);
}

auto readEpubInLibrary() -> void {
  const fs::path tmpShareAbs{fs::temp_directory_path()
                             / "epubworm_test_readEpubInLibrary"};
  fs::remove_all(tmpShareAbs);
  fs::create_directories(tmpShareAbs);

  const fs::path dataDirAbs{tmpShareAbs / "epubworm"};
  fs::create_directories(dataDirAbs);

  const fs::path libraryFileAbs{dataDirAbs / "library.xml"};
  ::initLibrary(libraryFileAbs);

  assert(::addToLibrary(epubsAbs / "metadata_paths.epub", tmpShareAbs)
             .has_value());
  assert(::addToLibrary(epubsAbs / "nonlinear_spine.epub", tmpShareAbs)
             .has_value());
  assert(::addToLibrary(epubsAbs / "nested_navigation.epub", tmpShareAbs)
             .has_value());
  assert(::addToLibrary(epubsAbs / "image_elements.epub", tmpShareAbs)
             .has_value());

  assert(!::readEpubInLibrary("000000", tmpShareAbs, 55));
  assert(!::readEpubInLibrary("", tmpShareAbs, 55));

  assert(::readEpubInLibrary("ded2eb", tmpShareAbs, 50));
  assert(::readEpubInLibrary("fd87f6", tmpShareAbs, 55));
  assert(::readEpubInLibrary("1dc96b", tmpShareAbs, 60));
  assert(::readEpubInLibrary("18c3f8", tmpShareAbs, 65));

  fs::create_directories(testOutputsAbs);
  const fs::path outputLibraryFileAbs{testOutputsAbs
                                      / "readEpubInLibrary_library.xml"};
  fs::copy_file(libraryFileAbs, outputLibraryFileAbs,
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

auto getLastRead() -> void {
  XMLDocument library{};
  library.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<library>"
                "<last-read id=\"11111111111111111111111111111111\"/>"
                "</library>");
  assert(!library.Error());
  assert(::getLastRead(library) == "11111111111111111111111111111111");

  XMLDocument noLibraryRoot{};
  noLibraryRoot.Parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                      "<not-library/>");
  assert(!noLibraryRoot.Error());
  try {
    (void)::getLastRead(noLibraryRoot);
    assert(false);
  }
  catch (const std::runtime_error& e) {
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
  }
  catch (const std::runtime_error& e) {
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
  }
  catch (const std::runtime_error& e) {
    assert(std::string_view{e.what()}
           == "`<last-read>` element missing `id` attribute");
  }
}
} // namespace test
