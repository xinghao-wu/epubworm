#include "tui.hpp"
#include "epub_parser.hpp"
#include "row_col_diacritics.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image/stb_image.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iconv.h>
#include <iostream>
#include <limits>
#include <locale>
#include <random>
// POSIX signal APIs expose declarations not guaranteed by <csignal>.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
// POSIX process APIs use FILE and fileno from this header.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#include <stdio.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <termios.h>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>
// POSIX wcwidth is declared by this header.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#include <wchar.h>

#ifdef __APPLE__
#include <crt_externs.h>
#endif

using namespace tinyxml2;
namespace fs = std::filesystem;

namespace {
// Raw mode setup must retain the original terminal state for later restoration.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
termios g_ogTermFlags{};
// Mutable sig_atomic_t is required for communication from the signal handler.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t g_winResize{0};
} // namespace

constexpr int wheelScrollLines{3};
constexpr int horizontalMarginChars{1};
constexpr int slowReadingWordsPerMinute{175};
constexpr int fastReadingWordsPerMinute{320};
constexpr int slowReadingCJKCharactersPerMinute{300};
constexpr int fastReadingCJKCharactersPerMinute{700};

namespace {
constexpr auto isInRange(std::uint32_t codePoint, std::uint32_t first,
                         std::uint32_t last) -> bool {
  return codePoint >= first && codePoint <= last;
}

constexpr auto isCombiningMark(std::uint32_t codePoint) -> bool {
  return isInRange(codePoint, 0x0300, 0x036F)
         || isInRange(codePoint, 0x1AB0, 0x1AFF)
         || isInRange(codePoint, 0x1DC0, 0x1DFF)
         || isInRange(codePoint, 0x20D0, 0x20FF)
         || isInRange(codePoint, 0xFE20, 0xFE2F)
         || isInRange(codePoint, 0x3099, 0x309A);
}

constexpr auto isVariationSelector(std::uint32_t codePoint) -> bool {
  return isInRange(codePoint, 0xFE00, 0xFE0F)
         || isInRange(codePoint, 0xE0100, 0xE01EF);
}

constexpr auto isCJKReadingCharacter(std::uint32_t codePoint) -> bool {
  const bool isHan{isInRange(codePoint, 0x3400, 0x4DBF)
                   || isInRange(codePoint, 0x4E00, 0x9FFF)
                   || isInRange(codePoint, 0xF900, 0xFAFF)
                   || isInRange(codePoint, 0x20000, 0x2EE5F)
                   || isInRange(codePoint, 0x2F800, 0x2FA1F)
                   || isInRange(codePoint, 0x30000, 0x323AF)};
  const bool isKana{
      isInRange(codePoint, 0x3041, 0x3096)
      || isInRange(codePoint, 0x309D, 0x309F)
      || (isInRange(codePoint, 0x30A1, 0x30FF) && codePoint != 0x30FB)
      || isInRange(codePoint, 0x31F0, 0x31FF)
      || isInRange(codePoint, 0x1B100, 0x1B16F)
      || isInRange(codePoint, 0xFF66, 0xFF9D)};
  const bool isIterationMark{isInRange(codePoint, 0x3005, 0x3007)
                             || codePoint == 0x303B};
  return isHan || isKana || isIterationMark;
}
} // namespace

auto loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols)
    -> void {
  if (id == 0) {
    throw std::runtime_error{"image id not in valid range"};
  }

  int xPixels{};
  int yPixels{};
  int sourceChannels{};

  if (stbi_info(imgAbs.c_str(), &xPixels, &yPixels, &sourceChannels) == 0) {
    throw std::runtime_error{stbi_failure_reason()};
  }

  const int loadedChannels{
      std::max(sourceChannels, static_cast<int>(STBI_rgb))};

  unsigned char* pixelData{stbi_load(imgAbs.c_str(), &xPixels, &yPixels,
                                     &sourceChannels, loadedChannels)};
  if (pixelData == nullptr) {
    throw std::runtime_error{stbi_failure_reason()};
  }

  const int pixelDataSize{xPixels * yPixels * loadedChannels};
  const std::string_view pixelDataView{reinterpret_cast<const char*>(pixelData),
                                       static_cast<std::size_t>(pixelDataSize)};

  fs::path tempDataDirAbs{"/dev/shm"};
  if (!fs::exists(tempDataDirAbs)) {
    tempDataDirAbs = fs::temp_directory_path();
  }
  const fs::path tempDataFileAbs{
      tempDataDirAbs
      / ("epubworm-img-data-" + std::to_string(id) + "-tty-graphics-protocol")};
  std::ofstream tempDataFile{tempDataFileAbs};
  if (!tempDataFile.is_open()) {
    throw std::runtime_error{"image temp data file failed to open"};
  }

  tempDataFile << pixelDataView;
  tempDataFile.close();
  stbi_image_free(pixelData);

  std::string graphicsEscCode{getGraphicsEscCode(
      tempDataFileAbs, loadedChannels, xPixels, yPixels, id, rows, cols)};
  if (inTmuxSession()) {
    wrapForTmuxPassthrough(graphicsEscCode);
  }
  std::cout << graphicsEscCode << std::flush;
}

auto displayLoadedImg(std::uint32_t id, int rows, int cols, std::string& out,
                      bool forITerm2) -> void {
  if (id < 1U || id > ((1U << 24U) - 1U)) {
    throw std::runtime_error{"image id not in valid range"};
  }

  const std::uint32_t idRed{(id >> 16U) & 255U};
  const std::uint32_t idGreen{(id >> 8U) & 255U};
  const std::uint32_t idBlue{id & 255U};
  const std::string idInFG{"[38;2;" + std::to_string(idRed) + ';'
                           + std::to_string(idGreen) + ';'
                           + std::to_string(idBlue) + 'm'};

  for (int r{0}; r < rows; ++r) {
    out += esc + idInFG;
    if (forITerm2) {
      for (int c{0}; c < cols; ++c) {
        out += imgCellPlaceholder + rowColDiacritics.data()[r]
               + rowColDiacritics.data()[c] + rowColDiacritics.front();
      }
    }
    else {
      out += imgCellPlaceholder + rowColDiacritics.data()[r];
      for (int c{1}; c < cols; ++c) {
        out += imgCellPlaceholder;
      }
    }
    out += esc + resetFG;
    out += '\n';
  }
}

auto displayImg(const fs::path& imgAbs, std::string& out, int rows, int cols)
    -> void {
  constexpr std::uint32_t minID{1};
  constexpr std::uint32_t maxID{(1U << 24U) - 1U};

  static std::mt19937 s_rng{std::random_device{}()};
  const std::uint32_t id{std::uniform_int_distribution{minID, maxID}(s_rng)};

  if (rows == 0 || cols == 0) {
    winsize winInfo{};
    ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);
    if (winInfo.ws_xpixel == 0 || winInfo.ws_ypixel == 0) {
      rows = 35;
      cols = 60;
    }
    else {
      const int cellXPix{winInfo.ws_xpixel / winInfo.ws_col};
      const int cellYPix{winInfo.ws_ypixel / winInfo.ws_row};

      int imgXPix{};
      int imgYPix{};
      int imgChannels{};
      stbi_info(imgAbs.c_str(), &imgXPix, &imgYPix, &imgChannels);

      const int rowsDesired{(imgYPix / cellYPix) + 1};
      const int colsDesired{(imgXPix / cellXPix) + 1};
      const int maxRows{std::max(static_cast<int>(winInfo.ws_row) - 2, 1)};
      const int maxCols{winInfo.ws_col - (horizontalMarginChars * 2)};
      const double rowShrinkMultiplier{maxRows
                                       / static_cast<double>(rowsDesired)};
      const double colShrinkMultiplier{maxCols
                                       / static_cast<double>(colsDesired)};
      rows = rowsDesired;
      cols = colsDesired;

      if (rowShrinkMultiplier < 1 || colShrinkMultiplier < 1) {
        if (rowShrinkMultiplier < colShrinkMultiplier) {
          rows = maxRows;
          cols = static_cast<int>(rowShrinkMultiplier * colsDesired) + 1;
        }
        else {
          cols = maxCols;
          rows = static_cast<int>(colShrinkMultiplier * rowsDesired) + 1;
        }
      }
    }
  }
  loadImg(imgAbs, id, rows, cols);
  displayLoadedImg(id, rows, cols, out, inITerm2Session());
}

auto getGraphicsEscCode(const fs::path& tempDataFileAbs, int channels,
                        int xPixels, int yPixels, std::uint32_t id, int rows,
                        int cols) -> std::string {
  std::string ctrlData{};
  ctrlData += "f=" + std::to_string(channels * 8) + ',';
  ctrlData += "s=" + std::to_string(xPixels) + ',';
  ctrlData += "v=" + std::to_string(yPixels) + ',';
  ctrlData += "i=" + std::to_string(id) + ',';
  ctrlData += "r=" + std::to_string(rows) + ',';
  ctrlData += "c=" + std::to_string(cols) + ',';
  ctrlData += "t=t,";
  ctrlData += "U=1,";
  ctrlData += "a=T,";
  ctrlData += "q=2";

  const std::string tempDataFileAbsEncoded{
      base64::to_base64(tempDataFileAbs.string())};

  return esc + "_G" + ctrlData + ';' + tempDataFileAbsEncoded + escEnd;
}

auto isTerm(FILE* fd) -> bool {
  return isatty(fileno(fd)) == 1;
}

auto boldColorIfTerm(FILE* fd, std::string_view fgColor) -> void {
  if (!isTerm(fd)) {
    return;
  }
  std::ostream& os{fd == stdout ? std::cout : std::cerr};
  os << esc << bold << esc << fgColor;
}

auto resetBoldColorIfTerm(FILE* fd) -> void {
  if (!isTerm(fd)) {
    return;
  }
  std::ostream& os{fd == stdout ? std::cout : std::cerr};
  os << esc << resetBold << esc << resetFG;
}

auto disableRawMode() -> void {
  std::cout << esc << disableDecimalReportingFormat;
  std::cout << esc << disableMouseEventReporting;
  std::cout << esc << showCursor;
  std::cout << std::flush;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_ogTermFlags) == -1) {
    throw std::runtime_error{"failed to restore original term settings"};
  }
}

auto enableRawMode() -> void {
  static bool hasRun{false};
  if (hasRun) {
    return;
  }
  hasRun = true;

  if (tcgetattr(STDIN_FILENO, &g_ogTermFlags) == -1) {
    throw std::runtime_error{"failed to get original terminal settings"};
  }

  if (std::atexit(disableRawMode) != 0) {
    throw std::runtime_error{"failed to register disableRawMode() to run "
                             "at program exit"};
  }

  termios rawTermFlags{g_ogTermFlags};
  // disable echo and canonical mode
  rawTermFlags.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
  // let `read()` return 0 every 100ms when not receiving input
  rawTermFlags.c_cc[VMIN] = 0;
  rawTermFlags.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermFlags) == -1) {
    throw std::runtime_error{"failed to set term settings to raw mode"};
  }

  std::cout << esc << enableDecimalReportingFormat;
  std::cout << esc << enableMouseEventReporting;
  std::cout << std::flush;

  registerSigwinchHandler();
}

auto utf8ToWide(std::string_view input) -> std::wstring {
  if (input.empty()) {
    return {};
  }

  iconv_t convDescriptor{iconv_open("WCHAR_T", "UTF-8")};
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  if (convDescriptor == reinterpret_cast<iconv_t>(-1)) {
    throw std::system_error{errno, std::generic_category(),
                            "iconv_open failed"};
  }

  char* inBuf{const_cast<char*>(input.data())};
  std::size_t inBytesLeft{input.size()};

  std::wstring output{};
  output.resize(input.size());
  char* outBuf{reinterpret_cast<char*>(output.data())};
  std::size_t outBytesLeft{output.size() * sizeof(wchar_t)};

  const std::size_t error{
      iconv(convDescriptor, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)};
  if (error == std::numeric_limits<std::size_t>::max()) {
    const int err{errno};
    iconv_close(convDescriptor);
    throw std::system_error{err, std::generic_category(),
                            "iconv conversion failed"};
  }

  iconv_close(convDescriptor);

  const std::size_t bytesWritten{(output.size() * sizeof(wchar_t))
                                 - outBytesLeft};
  output.resize(bytesWritten / sizeof(wchar_t));

  return output;
}

auto wideToUTF8(std::wstring_view input) -> std::string {
  if (input.empty()) {
    return {};
  }

  iconv_t convDescriptor{iconv_open("UTF-8", "WCHAR_T")};
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  if (convDescriptor == reinterpret_cast<iconv_t>(-1)) {
    throw std::system_error{errno, std::generic_category(),
                            "iconv_open failed"};
  }

  char* inBuf{const_cast<char*>(reinterpret_cast<const char*>(input.data()))};
  std::size_t inBytesLeft{input.size() * sizeof(wchar_t)};

  std::string output{};
  output.resize(input.size() * sizeof(wchar_t));
  char* outBuf{output.data()};
  std::size_t outBytesLeft{output.size()};

  const std::size_t error{
      iconv(convDescriptor, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)};
  if (error == std::numeric_limits<std::size_t>::max()) {
    const int err{errno};
    iconv_close(convDescriptor);
    throw std::system_error{err, std::generic_category(),
                            "iconv conversion failed"};
  }

  iconv_close(convDescriptor);

  const std::size_t bytesWritten{output.size() - outBytesLeft};
  output.resize(bytesWritten);

  return output;
}

auto getVisualLen(std::wstring_view str) -> int {
  int totalLen{0};
  for (const auto& ch : str) {
    int chLen{wcwidth(ch)};
    if (chLen == -1) {
      chLen = 0;
    }
    totalLen += chLen;
  }
  totalLen -= getInvisEscSeqLen(str);
  return totalLen;
}

auto getInvisEscSeqLen(std::wstring_view str) -> int {
  int totalLen{0};
  totalLen += getOccurrences<std::wstring_view>(str, L"\033\\") * 1;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[2J") * 3;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[H") * 2;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[2K") * 3;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[?25l") * 5;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[?25h") * 5;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[?1003h") * 7;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[?1003l") * 7;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[?1006h") * 7;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[?1006l") * 7;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[1m") * 3;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[22m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[3m") * 3;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[23m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[4m") * 3;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[24m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[33m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[31m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[32m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[34m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[35m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[36m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[37m") * 4;
  totalLen += getOccurrences<std::wstring_view>(str, L"\033[39m") * 4;
  return totalLen;
}

auto useSystemLocale() -> void {
  std::locale::global(std::locale(""));
  std::cout.imbue(std::locale{});
  std::cin.imbue(std::locale{});
}

auto collapseConsecutiveNewlines(std::string& str) -> void {
  constexpr std::string_view nonBreakingSpace{"\xC2\xA0"};
  constexpr std::string_view zeroWidthNonJoiner{"\xE2\x80\x8C"};
  std::size_t inputIndex{};
  std::size_t outputIndex{};
  int consecutiveNewlines{};
  while (inputIndex < str.size()) {
    const char ch{str[inputIndex]};
    if (ch == '\n') {
      if (consecutiveNewlines < 2) {
        str[outputIndex] = ch;
        ++outputIndex;
        ++consecutiveNewlines;
      }
      ++inputIndex;
      continue;
    }

    std::size_t whitespaceEnd{inputIndex};
    while (whitespaceEnd < str.size()) {
      if (str[whitespaceEnd] == ' ' || str[whitespaceEnd] == '\t') {
        ++whitespaceEnd;
      }
      else if (std::string_view{str}.substr(whitespaceEnd,
                                            nonBreakingSpace.size())
               == nonBreakingSpace) {
        whitespaceEnd += nonBreakingSpace.size();
      }
      else if (std::string_view{str}.substr(whitespaceEnd,
                                            zeroWidthNonJoiner.size())
               == zeroWidthNonJoiner) {
        whitespaceEnd += zeroWidthNonJoiner.size();
      }
      else {
        break;
      }
    }
    if (whitespaceEnd != inputIndex) {
      if (consecutiveNewlines != 0 && whitespaceEnd < str.size()
          && str[whitespaceEnd] == '\n') {
        inputIndex = whitespaceEnd;
        continue;
      }
      while (inputIndex < whitespaceEnd) {
        str[outputIndex] = str[inputIndex];
        ++outputIndex;
        ++inputIndex;
      }
      consecutiveNewlines = 0;
      continue;
    }

    str[outputIndex] = ch;
    ++outputIndex;
    ++inputIndex;
    consecutiveNewlines = 0;
  }
  str.resize(outputIndex);
}

auto wrapLines(std::string& str, int maxLen) -> void {
  for (std::size_t lineBeginIndex{0}, lineEndIndex{str.find('\n')};
       lineBeginIndex < str.size();
       lineBeginIndex = lineEndIndex + 1,
       lineEndIndex = str.find('\n', lineBeginIndex)) {

    if (lineEndIndex == lineBeginIndex) {
      continue;
    }
    if (lineEndIndex == std::string::npos) {
      lineEndIndex = str.size() - 1;
    }

    const std::string_view line{std::string_view{str}.substr(
        lineBeginIndex, lineEndIndex - lineBeginIndex + 1)};
    if (line.contains(imgCellPlaceholder)) {
      continue;
    }

    const std::wstring wideLine{utf8ToWide(line)};
    if (getVisualLen(wideLine) <= maxLen) {
      continue;
    }

    bool lineDone{false};
    std::size_t wideLineBreakIndex{};
    wideLineBreakIndex = wideLine.find_first_of(L" \n");
    while (wideLineBreakIndex != std::string::npos) {
      const std::wstring wideBeginToBreak{
          std::wstring_view{wideLine}.substr(0, wideLineBreakIndex + 1)};

      const int beginToBreakLen{getVisualLen(wideBeginToBreak)};
      if (beginToBreakLen > maxLen + 1) {
        break;
      }
      if (wideBeginToBreak.back() == L'\n') {
        if (beginToBreakLen <= maxLen) {
          lineDone = true;
          break;
        }
        if (beginToBreakLen == maxLen + 1) {
          break;
        }
      }
      wideLineBreakIndex =
          wideLine.find_first_of(L" \n", wideLineBreakIndex + 1);
    }
    wideLineBreakIndex = wideLine.rfind(L' ', wideLineBreakIndex - 1);

    if (lineDone) {
      continue;
    }

    std::size_t lineBreakIndex{};
    if (wideLineBreakIndex == std::string::npos) {
      std::size_t hardBreakWideIndex{};
      int hardBreakVisualLen{};
      for (std::size_t nextWideIndex{1}; nextWideIndex <= wideLine.size();
           ++nextWideIndex) {
        const int nextVisualLen{
            getVisualLen(std::wstring_view{wideLine}.substr(0, nextWideIndex))};
        if (nextVisualLen <= maxLen) {
          hardBreakWideIndex = nextWideIndex;
          hardBreakVisualLen = nextVisualLen;
        }
      }

      // A glyph can be wider than maxLen. Include it and any trailing
      // zero-width characters so wrapping always makes progress.
      if (hardBreakVisualLen == 0) {
        int firstGlyphVisualLen{};
        for (std::size_t nextWideIndex{hardBreakWideIndex + 1};
             nextWideIndex <= wideLine.size(); ++nextWideIndex) {
          const int nextVisualLen{getVisualLen(
              std::wstring_view{wideLine}.substr(0, nextWideIndex))};
          if (firstGlyphVisualLen == 0 && nextVisualLen > 0) {
            firstGlyphVisualLen = nextVisualLen;
          }
          if (nextVisualLen == firstGlyphVisualLen) {
            hardBreakWideIndex = nextWideIndex;
          }
        }
      }

      const std::string beginToLineBreak{wideToUTF8(
          std::wstring_view{wideLine}.substr(0, hardBreakWideIndex))};
      lineBreakIndex = lineBeginIndex + beginToLineBreak.size();
      str.insert(lineBreakIndex, 1, '\n');
    }
    else {
      const std::string beginToLineBreak{wideToUTF8(
          std::wstring_view{wideLine}.substr(0, wideLineBreakIndex + 1))};
      lineBreakIndex = lineBeginIndex + beginToLineBreak.size() - 1;
      str.replace(lineBreakIndex, 1, "\n");
    }

    lineEndIndex = lineBreakIndex;
  }
}

auto centerOnScreen(std::string& str, int maxLen) -> void {
  winsize winInfo{};
  ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

  for (std::size_t lineBeginIndex{0}; lineBeginIndex < str.size();
       lineBeginIndex = (str.find('\n', lineBeginIndex) == std::string::npos)
                            ? std::string::npos
                            : str.find('\n', lineBeginIndex) + 1) {

    const std::size_t lineEndIndex{str.find('\n', lineBeginIndex)};
    if (lineEndIndex == lineBeginIndex) {
      continue;
    }

    int contentWidth{};
    const std::string_view line{std::string_view{str}.substr(
        lineBeginIndex, lineEndIndex - lineBeginIndex + 1)};

    if (line.contains(imgCellPlaceholder)) {
      const int imgCols{
          getOccurrences<std::string_view>(line, imgCellPlaceholder)};
      contentWidth = imgCols;
    }
    else {
      contentWidth = maxLen;
    }

    const int paddingLen = (winInfo.ws_col - contentWidth) / 2;
    if (paddingLen <= 0) {
      continue;
    }
    str.insert(lineBeginIndex, static_cast<std::size_t>(paddingLen), ' ');
  }
}

namespace {
enum class MarkedAlignment : std::uint8_t {
  center,
  right,
};

auto alignMarkedText(std::string_view prefix, std::string_view postfix,
                     std::string& str, int maxLen, MarkedAlignment alignment)
    -> void {
  if (prefix.empty() || postfix.empty()) {
    return;
  }

  std::size_t rangeBegin{str.find(prefix)};
  while (rangeBegin != std::string::npos) {
    const std::size_t postfixBegin{
        str.find(postfix, rangeBegin + prefix.size())};
    if (postfixBegin == std::string::npos) {
      break;
    }
    std::size_t rangeEnd{postfixBegin + postfix.size() - 1};

    std::size_t lineBegin{rangeBegin};
    while (lineBegin <= rangeEnd) {
      const std::size_t newline{str.find('\n', lineBegin)};
      const bool finalLine{newline == std::string::npos || newline > rangeEnd};
      std::size_t lineEnd{finalLine ? rangeEnd : newline};

      const std::string_view line{
          std::string_view{str}.substr(lineBegin, lineEnd - lineBegin + 1)};
      const std::wstring wideLine{utf8ToWide(line)};
      const int lineVisualLen{getVisualLen(wideLine)};
      const int remainingWidth{maxLen - lineVisualLen};
      int paddingLen{};
      if (!line.contains(imgCellPlaceholder) && lineVisualLen > 0
          && remainingWidth > 0) {
        paddingLen = alignment == MarkedAlignment::center ? remainingWidth / 2
                                                          : remainingWidth;
      }

      if (paddingLen > 0) {
        const std::size_t padding{static_cast<std::size_t>(paddingLen)};
        str.insert(lineBegin, padding, ' ');
        lineEnd += padding;
        rangeEnd += padding;
      }

      if (finalLine) {
        break;
      }
      lineBegin = lineEnd + 1;
    }

    rangeBegin = str.find(prefix, rangeEnd + 1);
  }
}
} // namespace

auto centerJustify(std::string_view prefix, std::string_view postfix,
                   std::string& str, int maxLen) -> void {
  alignMarkedText(prefix, postfix, str, maxLen, MarkedAlignment::center);
}

auto rightJustify(std::string_view prefix, std::string_view postfix,
                  std::string& str, int maxLen) -> void {
  alignMarkedText(prefix, postfix, str, maxLen, MarkedAlignment::right);
}

auto readRawInput() -> std::tuple<Key, int, int> {
  ssize_t err{};
  char startCh{};
  while ((err = read(STDIN_FILENO, &startCh, 1)) != 1 && g_winResize == 0) {
    if (err == -1 && errno != EAGAIN && errno != EINTR) {
      throw std::system_error{errno, std::generic_category(),
                              "raw mode read key errored"};
    }
  }
  if (g_winResize == 1) {
    g_winResize = 0;
    return {specKey::winResize, 0, 0};
  }
  if (startCh != '\033') {
    return {startCh, 0, 0};
  }

  std::string seq{};
  seq.resize(100);
  std::size_t i{0};
  while (read(STDIN_FILENO, &seq[i], 1) == 1) {
    switch (seq[i]) {
    case '~':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'H':
    case 'F':
    case 'M':
    case 'm':
      ++i;
      goto exit_loop;
    default:
      break;
    }
    ++i;
  }
exit_loop:
  seq.resize(i);

  if (seq.empty()) {
    return {'\033', 0, 0};
  }

  if (seq == "[5~") {
    return {specKey::pgUp, 0, 0};
  }
  if (seq == "[6~") {
    return {specKey::pgDown, 0, 0};
  }
  if (seq == "[A") {
    return {specKey::arrowUp, 0, 0};
  }
  if (seq == "[B") {
    return {specKey::arrowDown, 0, 0};
  }
  if (seq == "[C") {
    return {specKey::arrowRight, 0, 0};
  }
  if (seq == "[D") {
    return {specKey::arrowLeft, 0, 0};
  }
  if (seq == "[1~" || esc == "[7~" || esc == "[H" || esc == "OH") {
    return {specKey::home, 0, 0};
  }
  if (seq == "[4~" || esc == "[8~" || esc == "[F" || esc == "OF") {
    return {specKey::end, 0, 0};
  }

  if (seq.starts_with("[<")) {
    assert(seq.find_first_of("mM") == seq.size() - 1
           && "received incomplete or multiple mouse actions");

    const std::size_t firstSemicolonIndex{findNth(seq, ";", 1)};
    const std::size_t secSemicolonIndex{findNth(seq, ";", 2)};
    const int action{std::stoi(seq.substr(2, firstSemicolonIndex - 2))};
    const int col{std::stoi(seq.substr(
        firstSemicolonIndex + 1, secSemicolonIndex - firstSemicolonIndex - 1))};
    const int row{
        std::stoi(seq.substr(secSemicolonIndex + 1,
                             seq.find_first_of("mM") - secSemicolonIndex - 1))};

    if (seq.back() == 'm') {
      if (action == 0) {
        return {specKey::leftClickRelease, row, col};
      }
      if (action == 2) {
        return {specKey::rightClickRelease, row, col};
      }
    }
    if (seq.back() == 'M') {
      if (action == 64) {
        return {specKey::wheelUp, row, col};
      }
      if (action == 65) {
        return {specKey::wheelDown, row, col};
      }
    }
    return {specKey::unknown, row, col};
  }
  return {specKey::unknown, 0, 0};
}

auto eraseScreen() -> void {
  winsize winInfo{};
  ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

  std::cout << esc << posCursorTopLeft;
  const int windowRows{static_cast<int>(winInfo.ws_row)};
  for (int row{0}; row < windowRows; ++row) {
    std::cout << esc << eraseLine;
    if (row < windowRows - 1) {
      std::cout << '\n';
    }
  }
  std::cout << esc << posCursorTopLeft;
}

auto styleEachLineIndividually(std::string& str, std::string_view style,
                               std::string_view resetStyle) -> void {
  std::size_t styleBeginIndex{str.find(style)};
  std::size_t styleEndIndex{str.find(resetStyle, styleBeginIndex + style.size())
                            + resetStyle.size() - 1};
  while (styleBeginIndex != std::string::npos) {
    std::size_t styleNewlineIndex{str.find('\n', styleBeginIndex)};
    while (styleNewlineIndex < styleEndIndex) {
      str.insert(styleNewlineIndex, resetStyle);
      styleNewlineIndex += resetStyle.size();
      styleEndIndex += resetStyle.size();
      std::size_t nextLineStyleIndex{styleNewlineIndex + 1};
      while (nextLineStyleIndex < str.size()
             && str.at(nextLineStyleIndex) == ' ') {
        ++nextLineStyleIndex;
      }
      str.insert(nextLineStyleIndex, style);
      styleEndIndex += style.size();

      styleNewlineIndex = str.find('\n', styleNewlineIndex + 1);
    }

    styleBeginIndex = str.find(style, styleEndIndex + 1);
    styleEndIndex = str.find(resetStyle, styleBeginIndex + style.size())
                    + resetStyle.size() - 1;
  }
}

auto processContentText(std::string& str, int maxLen) -> void {
  expandEllipsesAndTabs(str);
  wrapLines(str, maxLen);
  centerJustify(centerAlignBegin, centerAlignEnd, str, maxLen);
  rightJustify(rightAlignBegin, rightAlignEnd, str, maxLen);
  findAndReplaceAll(str, centerAlignBegin, "");
  findAndReplaceAll(str, centerAlignEnd, "");
  findAndReplaceAll(str, rightAlignBegin, "");
  findAndReplaceAll(str, rightAlignEnd, "");
  collapseConsecutiveNewlines(str);
  centerOnScreen(str, maxLen);
  styleEachLineIndividually(str, esc + bold, esc + resetBold);
  styleEachLineIndividually(str, esc + italic, esc + resetItalic);
  styleEachLineIndividually(str, esc + underline, esc + resetUnderline);
  styleEachLineIndividually(str, esc + yellowFG, esc + resetFG);
  styleEachLineIndividually(str, esc + cyanFG, esc + resetFG);
  styleEachLineIndividually(str, esc + redFG, esc + resetFG);
  styleEachLineIndividually(str, esc + greenFG, esc + resetFG);
  styleEachLineIndividually(str, esc + blueFG, esc + resetFG);
  styleEachLineIndividually(str, esc + magentaFG, esc + resetFG);
  styleEachLineIndividually(str, esc + lightGrayFG, esc + resetFG);
}

auto getChapterReadingStats(std::string_view chapter) -> ChapterReadingStats {
  const std::wstring wideChapter{utf8ToWide(chapter)};
  ChapterReadingStats stats{};
  bool inWord{false};
  for (std::size_t i{}; i < wideChapter.size();) {
    if (wideChapter[i] == L'\033') {
      if (i + 1 < wideChapter.size() && wideChapter[i + 1] == L'[') {
        i += 2;
        while (i < wideChapter.size()
               && (wideChapter[i] < L'@' || wideChapter[i] > L'~')) {
          ++i;
        }
        i += static_cast<std::size_t>(i < wideChapter.size());
      }
      else {
        ++i;
        while (i + 1 < wideChapter.size()
               && (wideChapter[i] != L'\033' || wideChapter[i + 1] != L'\\')) {
          ++i;
        }
        i = std::min(i + 2, wideChapter.size());
      }
      continue;
    }

    const wchar_t ch{wideChapter[i]};
    const auto codePoint{static_cast<std::uint32_t>(ch)};
    if (isCombiningMark(codePoint) || isVariationSelector(codePoint)) {
      ++i;
      continue;
    }
    if (isCJKReadingCharacter(codePoint)) {
      ++stats.cjkCharacters;
      inWord = false;
      ++i;
      continue;
    }

    const bool isAlphanumeric{std::iswalnum(static_cast<std::wint_t>(ch)) != 0};
    if (isAlphanumeric && !inWord) {
      ++stats.words;
    }
    inWord = isAlphanumeric
             || (inWord
                 && (ch == L'\'' || ch == L'’' || ch == L'-' || ch == L'‐'
                     || ch == L'‑'));
    ++i;
  }
  return stats;
}

auto getChapterProgressIndicator(int screenTopLine, int screenRows,
                                 int screenCols, int chapterLines,
                                 const ChapterReadingStats& chapterReadingStats,
                                 std::string_view title) -> std::string {
  double progress{1};
  if (chapterLines > screenRows) {
    const int scrollableLines{chapterLines - screenRows};
    progress = std::clamp(
        static_cast<double>(screenTopLine - 1) / scrollableLines, 0.0, 1.0);
  }
  const int percent{static_cast<int>(std::lround(progress * 100))};
  const int remainingLines{
      std::clamp(chapterLines - screenTopLine + 1, 0, chapterLines)};
  const double remainingFraction{
      chapterLines == 0 ? 0
                        : static_cast<double>(remainingLines) / chapterLines};
  const double remainingWords{chapterReadingStats.words * remainingFraction};
  const double remainingCJKCharacters{chapterReadingStats.cjkCharacters
                                      * remainingFraction};
  const int minMinutesLeft{static_cast<int>(std::ceil(
      (remainingWords / fastReadingWordsPerMinute)
      + (remainingCJKCharacters / fastReadingCJKCharactersPerMinute)))};
  const int maxMinutesLeft{static_cast<int>(std::ceil(
      (remainingWords / slowReadingWordsPerMinute)
      + (remainingCJKCharacters / slowReadingCJKCharactersPerMinute)))};

  const std::wstring wideTitle{utf8ToWide(title)};
  const std::wstring progressText{std::to_wstring(percent)
                                  + L"% chapter progress"};
  const std::wstring timeText{std::to_wstring(minMinutesLeft) + L"-"
                              + std::to_wstring(maxMinutesLeft) + L" min left"};
  const std::wstring fullRight{progressText + L" · " + timeText + L" ─"};
  std::wstring right{fullRight};

  const int availableCols{std::max(screenCols, 0)};
  constexpr int leftPrefixCols{2};
  const int contentAvailableCols{std::max(availableCols - leftPrefixCols, 0)};
  int rightCols{getVisualLen(right)};
  const bool showTitle{!wideTitle.empty()
                       && contentAvailableCols >= rightCols + 5};
  if (!showTitle && rightCols > contentAvailableCols) {
    std::wstring fittedRight{};
    rightCols = 0;
    for (auto it{right.rbegin()}; it != right.rend(); ++it) {
      const int chCols{std::max(wcwidth(*it), 0)};
      if (rightCols + chCols > contentAvailableCols) {
        break;
      }
      fittedRight.insert(fittedRight.begin(), *it);
      rightCols += chCols;
    }
    right = std::move(fittedRight);
  }

  std::wstring fittedTitle{};
  int titleCols{};
  int middleRuleCols{};
  if (showTitle) {
    const int titleAvailableCols{contentAvailableCols - rightCols - 4};
    const bool titleTruncated{getVisualLen(wideTitle) > titleAvailableCols};
    const int titleTextAvailableCols{titleAvailableCols
                                     - static_cast<int>(titleTruncated)};
    for (const wchar_t ch : wideTitle) {
      const int chCols{std::max(wcwidth(ch), 0)};
      if (titleCols + chCols > titleTextAvailableCols) {
        break;
      }
      fittedTitle += ch;
      titleCols += chCols;
    }
    if (titleTruncated) {
      fittedTitle += L'…';
      ++titleCols;
    }
    middleRuleCols = contentAvailableCols - titleCols - rightCols - 2;
  }

  std::wstring styledLeft{L"─ "};
  if (!fittedTitle.empty()) {
    styledLeft += L"\033[33m";
    styledLeft += fittedTitle;
    styledLeft += L"\033[39m";
    styledLeft += L' ';
    styledLeft.append(static_cast<std::size_t>(middleRuleCols), L'─');
    styledLeft += L' ';
  }

  const std::size_t progressEnd{progressText.size()};
  const std::size_t timeBegin{progressEnd + 3};
  const std::size_t timeEnd{timeBegin + timeText.size()};
  const std::size_t rightOffset{fullRight.size() - right.size()};
  std::wstring styledRight{};
  int activeStyle{};
  for (std::size_t i{}; i < right.size(); ++i) {
    const std::size_t originalIndex{rightOffset + i};
    int style{};
    if (originalIndex < progressEnd) {
      style = 1;
    }
    else if (originalIndex >= timeBegin && originalIndex < timeEnd) {
      style = 2;
    }
    if (style != activeStyle) {
      if (activeStyle != 0) {
        styledRight += L"\033[39m";
      }
      if (style == 1) {
        styledRight += L"\033[32m";
      }
      else if (style == 2) {
        styledRight += L"\033[34m";
      }
      activeStyle = style;
    }
    styledRight += right[i];
  }
  if (activeStyle != 0) {
    styledRight += L"\033[39m";
  }
  if (fittedTitle.empty()) {
    styledRight.append(
        static_cast<std::size_t>(std::max(contentAvailableCols - rightCols, 0)),
        L'─');
  }

  const std::wstring status{styledLeft + styledRight};

  return esc + resetFG + wideToUTF8(status);
}

auto displayChapter(const fs::path& chapterAbs, std::string_view title,
                    double iniProg, int desiredMaxLen)
    -> std::pair<ChapterExit, double> {
  winsize winInfo{};
  std::string chapter{};
  int chapterLines{};
  int screenRows{};
  int screenTopLine{};
  int screenBotLine{};
  setUpDisplayChapter(chapterAbs, iniProg, desiredMaxLen, winInfo, chapter,
                      chapterLines, screenRows, screenTopLine, screenBotLine);
  const ChapterReadingStats chapterReadingStats{
      getChapterReadingStats(chapter)};

  while (true) {
    std::size_t dispBeginIndex{};
    if (screenTopLine == 1) {
      dispBeginIndex = 0;
    }
    else {
      dispBeginIndex = findNth(chapter, "\n", screenTopLine - 1) + 1;
    }
    const std::size_t dispEndIndex{findNth(chapter, "\n", screenBotLine) - 1};
    const std::string_view dispView{std::string_view{chapter}.substr(
        dispBeginIndex, dispEndIndex - dispBeginIndex + 1)};

    eraseScreen();
    std::cout << dispView << esc << '[' << winInfo.ws_row << ";1H"
              << getChapterProgressIndicator(screenTopLine, screenRows,
                                             static_cast<int>(winInfo.ws_col),
                                             chapterLines, chapterReadingStats,
                                             title)
              << std::flush;
    // Sometimes, images on right-side tmux panes are broken until redraw.
    if (inTmuxSession()) {
      execute(std::vector<std::string>{"tmux", "refresh-client"});
    }

    const double prog{static_cast<double>(screenTopLine) / chapterLines};

    while (true) {
      std::tuple<Key, int, int> input{readRawInput()};
      Key translatedInputKey{std::get<0>(input)};
      if (translatedInputKey == specKey::leftClickRelease) {
        if (std::get<2>(input) <= winInfo.ws_col / 2) {
          translatedInputKey = 'h';
        }
        else {
          translatedInputKey = 'l';
        }
      }

      switch (translatedInputKey) {
      case 't':
      case '\t':
      case '\033':
        return {ChapterExit::toc, prog};
      case 'q':
        return {ChapterExit::quit, prog};
      case 'h':
      case 'b':
      case ctrlB:
      case specKey::arrowLeft:
      case specKey::pgUp:
        if (screenTopLine == 1) {
          return {ChapterExit::prev, prog};
        }
        screenTopLine -= screenRows;
        snapTopLineToBound(screenTopLine);
        screenBotLine = calcBotLineFromTopLine(screenTopLine, screenRows);
        snapBotLineToBound(screenBotLine, chapterLines);
        goto redraw_screen;
      case 'l':
      case 'f':
      case ctrlF:
      case ' ':
      case specKey::arrowRight:
      case specKey::pgDown:
        if (screenBotLine == chapterLines) {
          return {ChapterExit::next, prog};
        }
        screenBotLine += screenRows;
        snapBotLineToBound(screenBotLine, chapterLines);
        screenTopLine = calcTopLineFromBotLine(screenBotLine, screenRows);
        snapTopLineToBound(screenTopLine);
        goto redraw_screen;
      case 'u':
      case ctrlU:
        if (screenTopLine == 1) {
          return {ChapterExit::prev, prog};
        }
        screenTopLine -= screenRows / 2;
        snapTopLineToBound(screenTopLine);
        screenBotLine = calcBotLineFromTopLine(screenTopLine, screenRows);
        snapBotLineToBound(screenBotLine, chapterLines);
        goto redraw_screen;
      case 'd':
      case ctrlD:
        if (screenBotLine == chapterLines) {
          return {ChapterExit::next, prog};
        }
        screenBotLine += screenRows / 2;
        snapBotLineToBound(screenBotLine, chapterLines);
        screenTopLine = calcTopLineFromBotLine(screenBotLine, screenRows);
        snapTopLineToBound(screenTopLine);
        goto redraw_screen;
      case 'k':
      case specKey::arrowUp:
        if (screenTopLine == 1) {
          return {ChapterExit::prev, prog};
        }
        --screenTopLine;
        --screenBotLine;
        goto redraw_screen;
      case 'j':
      case specKey::arrowDown:
        if (screenBotLine == chapterLines) {
          return {ChapterExit::next, prog};
        }
        ++screenTopLine;
        ++screenBotLine;
        goto redraw_screen;
      case specKey::wheelUp:
        if (screenTopLine == 1) {
          break;
        }
        screenTopLine -= wheelScrollLines;
        snapTopLineToBound(screenTopLine);
        screenBotLine = calcBotLineFromTopLine(screenTopLine, screenRows);
        snapBotLineToBound(screenBotLine, chapterLines);
        goto redraw_screen;
      case specKey::wheelDown:
        if (screenBotLine == chapterLines) {
          break;
        }
        screenBotLine += wheelScrollLines;
        snapBotLineToBound(screenBotLine, chapterLines);
        screenTopLine = calcTopLineFromBotLine(screenBotLine, screenRows);
        snapTopLineToBound(screenTopLine);
        goto redraw_screen;
      case 'g':
      case specKey::home:
        if (screenTopLine != 1) {
          screenTopLine = 1;
          screenBotLine = calcBotLineFromTopLine(screenTopLine, screenRows);
          snapBotLineToBound(screenBotLine, chapterLines);
          goto redraw_screen;
        }
        break;
      case 'G':
      case specKey::end:
        if (screenBotLine != chapterLines) {
          screenBotLine = chapterLines;
          screenTopLine = calcTopLineFromBotLine(screenBotLine, screenRows);
          snapTopLineToBound(screenTopLine);
          goto redraw_screen;
        }
        break;
      case specKey::winResize:
        setUpDisplayChapter(chapterAbs, prog, desiredMaxLen, winInfo, chapter,
                            chapterLines, screenRows, screenTopLine,
                            screenBotLine);
        goto redraw_screen;
      default:
        break;
      }
    }
redraw_screen:
  }
}

auto setUpDisplayChapter(const fs::path& chapterAbs, double prog,
                         int desiredMaxLen, winsize& winInfo,
                         std::string& chapter, int& chapterLines,
                         int& screenRows, int& screenTopLine,
                         int& screenBotLine) -> void {

  ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);
  screenRows = std::max(static_cast<int>(winInfo.ws_row) - 1, 1);

  chapter.clear();
  parseChapter(chapterAbs, chapter);
  const int maxLen{
      std::min(desiredMaxLen,
               static_cast<int>(winInfo.ws_col - (horizontalMarginChars * 2)))};
  processContentText(chapter, maxLen);

  chapterLines = getOccurrences<std::string_view>(chapter, "\n");

  screenTopLine = static_cast<int>(std::lround(prog * chapterLines));
  snapTopLineToBound(screenTopLine);

  screenBotLine = calcBotLineFromTopLine(screenTopLine, screenRows);
  snapBotLineToBound(screenBotLine, chapterLines);
  screenTopLine = calcTopLineFromBotLine(screenBotLine, screenRows);
  snapTopLineToBound(screenTopLine);
}

auto snapTopLineToBound(int& screenTopLine) -> void {
  screenTopLine = std::max(screenTopLine, 1);
}

auto snapBotLineToBound(int& screenBotLine, int chapterLines) -> void {
  screenBotLine = std::min(screenBotLine, chapterLines);
}

auto calcBotLineFromTopLine(int screenTopLine, int screenRows) -> int {
  return screenTopLine + screenRows - 1;
}

auto calcTopLineFromBotLine(int screenBotLine, int screenRows) -> int {
  return screenBotLine - screenRows + 1;
}

auto execute(const std::vector<std::string>& argV) -> std::string {
  if (argV.empty() || argV.front().empty()) {
    throw std::invalid_argument{"execute() cmd cannot be empty"};
  }

  std::vector<char*> posixAPIArgV{};
  posixAPIArgV.reserve(argV.size() + 1);
  for (const auto& arg : argV) {
    posixAPIArgV.push_back(const_cast<char*>(arg.c_str()));
  }
  posixAPIArgV.push_back(nullptr);

  std::array<int, 2> pipeFds{};
  if (pipe(pipeFds.data()) == -1) {
    throw std::system_error{errno, std::generic_category(),
                            "failed to create pipe for cmd: " + argV.front()};
  }

  posix_spawn_file_actions_t fileActions{};
  int faStatus{posix_spawn_file_actions_init(&fileActions)};
  if (faStatus != 0) {
    close(pipeFds[0]);
    close(pipeFds[1]);
    throw std::system_error{faStatus, std::generic_category(),
                            "failed to init file actions for cmd: "
                                + argV.front()};
  }

  faStatus =
      posix_spawn_file_actions_adddup2(&fileActions, pipeFds[1], STDOUT_FILENO);
  if (faStatus != 0) {
    posix_spawn_file_actions_destroy(&fileActions);
    close(pipeFds[0]);
    close(pipeFds[1]);
    throw std::system_error{faStatus, std::generic_category(),
                            "failed to dup2 pipe write end for cmd: "
                                + argV.front()};
  }
  faStatus = posix_spawn_file_actions_addclose(&fileActions, pipeFds[0]);
  if (faStatus != 0) {
    posix_spawn_file_actions_destroy(&fileActions);
    close(pipeFds[0]);
    close(pipeFds[1]);
    throw std::system_error{faStatus, std::generic_category(),
                            "failed to close pipe read end for cmd: "
                                + argV.front()};
  }
  faStatus = posix_spawn_file_actions_addclose(&fileActions, pipeFds[1]);
  if (faStatus != 0) {
    posix_spawn_file_actions_destroy(&fileActions);
    close(pipeFds[0]);
    close(pipeFds[1]);
    throw std::system_error{faStatus, std::generic_category(),
                            "failed to close pipe write end for cmd: "
                                + argV.front()};
  }

  pid_t pid{};
#ifdef __APPLE__
  char* const* const environment{*_NSGetEnviron()};
#else
  char* const* const environment{environ};
#endif
  const int spawnStatus{posix_spawnp(&pid, posixAPIArgV.front(), &fileActions,
                                     nullptr, posixAPIArgV.data(),
                                     environment)};
  posix_spawn_file_actions_destroy(&fileActions);
  close(pipeFds[1]); // parent closes write end so `read()` can hit EOF
  if (spawnStatus != 0) {
    close(pipeFds[0]);
    throw std::system_error{spawnStatus, std::generic_category(),
                            "failed to spawn cmd: " + argV.front()};
  }

  std::string output{};
  std::string buf(4096, '\0');
  while (true) {
    const ssize_t bytesRead{read(pipeFds[0], buf.data(), buf.size())};
    if (bytesRead > 0) {
      output.append(buf, 0, static_cast<std::size_t>(bytesRead));
    }
    else if (bytesRead == 0) {
      break; // EOF
    }
    else if (errno != EAGAIN && errno != EINTR) {
      const int err{errno};
      close(pipeFds[0]);
      throw std::system_error{err, std::generic_category(),
                              "error reading cmd output: " + argV.front()};
    }
  }
  close(pipeFds[0]);

  int waitStatus{};
  while (waitpid(pid, &waitStatus, 0) == -1) {
    if (errno != EAGAIN && errno != EINTR) {
      throw std::system_error{errno, std::generic_category(),
                              "error waiting for cmd: " + argV.front()};
    }
  }
  if (!WIFEXITED(waitStatus) || WEXITSTATUS(waitStatus) != 0) {
    throw std::runtime_error{"cmd did not exit properly: " + argV.front()};
  }

  return output;
}

extern "C" auto handleSigwinch([[maybe_unused]] int signal) -> void {
  g_winResize = 1;
}

auto registerSigwinchHandler() -> void {
  struct sigaction sigAct{};
  sigAct.sa_handler = handleSigwinch;
  sigemptyset(&sigAct.sa_mask); // block no other signals when handling
  sigAct.sa_flags = SA_RESTART; // restart interrupted system calls

  if (sigaction(SIGWINCH, &sigAct, nullptr) == -1) {
    throw std::runtime_error{"failed to register SIGWINCH handler"};
  }
}

auto tocDataToString(const TocData& data, std::string& str) -> void {
  for (const auto& navPoint : data) {
    str += navPoint.first + "\n\n";
  }
  str.pop_back();

  str += centerAlignBegin;
  str += esc + redFG;
  str += esc + bold;
  str += "---";
  str += esc + resetFG;
  str += esc + resetBold;
  str += centerAlignEnd;
  str += '\n';
}

auto getTOCStatusLine(int screenCols, std::string_view title) -> std::string {
  const std::wstring wideTitle{utf8ToWide(title)};
  const std::wstring fullRight{L"Table of Contents ─"};
  std::wstring right{fullRight};

  const int availableCols{std::max(screenCols, 0)};
  constexpr int leftPrefixCols{2};
  const int contentAvailableCols{std::max(availableCols - leftPrefixCols, 0)};
  int rightCols{getVisualLen(right)};
  const bool showTitle{!wideTitle.empty()
                       && contentAvailableCols >= rightCols + 5};
  if (!showTitle && rightCols > contentAvailableCols) {
    std::wstring fittedRight{};
    rightCols = 0;
    for (auto it{right.rbegin()}; it != right.rend(); ++it) {
      const int chCols{std::max(wcwidth(*it), 0)};
      if (rightCols + chCols > contentAvailableCols) {
        break;
      }
      fittedRight.insert(fittedRight.begin(), *it);
      rightCols += chCols;
    }
    right = std::move(fittedRight);
  }

  std::wstring fittedTitle{};
  int titleCols{};
  int middleRuleCols{};
  if (showTitle) {
    const int titleAvailableCols{contentAvailableCols - rightCols - 4};
    const bool titleTruncated{getVisualLen(wideTitle) > titleAvailableCols};
    const int titleTextAvailableCols{titleAvailableCols
                                     - static_cast<int>(titleTruncated)};
    for (const wchar_t ch : wideTitle) {
      const int chCols{std::max(wcwidth(ch), 0)};
      if (titleCols + chCols > titleTextAvailableCols) {
        break;
      }
      fittedTitle += ch;
      titleCols += chCols;
    }
    if (titleTruncated) {
      fittedTitle += L'…';
      ++titleCols;
    }
    middleRuleCols = contentAvailableCols - titleCols - rightCols - 2;
  }

  std::wstring status{L"─ "};
  if (!fittedTitle.empty()) {
    status += L"\033[33m";
    status += fittedTitle;
    status += L"\033[39m ";
    status.append(static_cast<std::size_t>(middleRuleCols), L'─');
    status += L' ';
  }

  const std::size_t rightOffset{fullRight.size() - right.size()};
  const std::size_t textEnd{fullRight.size() - 2};
  if (rightOffset < textEnd) {
    const std::size_t visibleTextChars{
        std::min(right.size(), textEnd - rightOffset)};
    status += L"\033[35m";
    status += right.substr(0, visibleTextChars);
    status += L"\033[39m";
    status += right.substr(visibleTextChars);
  }
  else {
    status += right;
  }
  if (fittedTitle.empty()) {
    status.append(
        static_cast<std::size_t>(std::max(contentAvailableCols - rightCols, 0)),
        L'─');
  }

  return esc + resetFG + wideToUTF8(status);
}

auto inTmuxSession() -> bool {
  const char* tmux{std::getenv("TMUX")};
  if (tmux != nullptr && *tmux != '\0') {
    return true;
  }

  const char* termProgram{std::getenv("TERM_PROGRAM")};
  return termProgram != nullptr && std::string_view{termProgram} == "tmux";
}

auto inITerm2Session() -> bool {
  const char* termProgram{std::getenv("TERM_PROGRAM")};
  if (termProgram != nullptr && *termProgram != '\0') {
    const std::string_view termProgramView{termProgram};
    if (termProgramView == "iTerm.app") {
      return true;
    }
    if (termProgramView != "tmux") {
      return false;
    }
  }

  const char* lcTerminal{std::getenv("LC_TERMINAL")};
  return lcTerminal != nullptr && std::string_view{lcTerminal} == "iTerm2";
}

auto displayTOC(const TocData& tocData, std::string_view title,
                int desiredMaxLen, int selectedNavPointIndex) -> fs::path {
  winsize winInfo{};
  std::string tocStr{};
  int tocLines{};
  int screenRows{};
  setUpDisplayTOC(tocData, desiredMaxLen, winInfo, tocStr, tocLines,
                  screenRows);

  while (true) {
    if (!(selectedNavPointIndex < std::ssize(tocData))) {
      throw std::logic_error{"selected nav point out of bounds"};
    }

    std::size_t selectionBeginIndex{};
    if (selectedNavPointIndex != 0) {
      selectionBeginIndex = findNth(tocStr, "\n\n", selectedNavPointIndex) + 2;
    }
    std::size_t selectionEndIndex{};
    if (selectedNavPointIndex == std::ssize(tocData) - 1) {
      selectionEndIndex = tocStr.rfind('\n', tocStr.rfind('\n') - 1) - 1;
    }
    else {
      selectionEndIndex =
          findNth(tocStr, "\n\n", selectedNavPointIndex + 1) - 1;
    }
    const int selectionBeginLine{
        getOccurrences<std::string_view>(
            std::string_view{tocStr}.substr(0, selectionBeginIndex + 1), "\n")
        + 1};
    const int selectionEndLine{
        getOccurrences<std::string_view>(
            std::string_view{tocStr}.substr(0, selectionEndIndex + 1), "\n")
        + 1};
    const int selectionLines{selectionEndLine - selectionBeginLine + 1};
    const int nonSelectionLines{screenRows - selectionLines};

    int screenTopLine{selectionBeginLine - (nonSelectionLines / 2)};
    snapTopLineToBound(screenTopLine);
    int screenBotLine{calcBotLineFromTopLine(screenTopLine, screenRows)};
    snapBotLineToBound(screenBotLine, tocLines);
    screenTopLine = calcTopLineFromBotLine(screenBotLine, screenRows);
    snapTopLineToBound(screenTopLine);

    std::size_t dispBeginIndex{};
    if (screenTopLine == 1) {
      dispBeginIndex = 0;
    }
    else {
      dispBeginIndex = findNth(tocStr, "\n", screenTopLine - 1) + 1;
    }
    const std::size_t dispEndIndex{findNth(tocStr, "\n", screenBotLine) - 1};

    const std::string_view dispBeforeSelection{std::string_view{tocStr}.substr(
        dispBeginIndex, selectionBeginIndex - dispBeginIndex)};
    const std::string_view dispSelection{std::string_view{tocStr}.substr(
        selectionBeginIndex, selectionEndIndex - selectionBeginIndex + 1)};
    const std::string_view dispAfterSelection{std::string_view{tocStr}.substr(
        selectionEndIndex + 1, dispEndIndex - selectionEndIndex)};

    eraseScreen();
    std::cout << dispBeforeSelection;
    std::cout << esc << cyanFG;
    std::cout << esc << bold;
    std::cout << dispSelection;
    std::cout << esc << resetFG;
    std::cout << esc << resetBold;
    std::cout << dispAfterSelection;
    std::cout << esc << '[' << winInfo.ws_row << ";1H"
              << getTOCStatusLine(static_cast<int>(winInfo.ws_col), title);
    std::cout << std::flush;

    while (true) {
      std::tuple<Key, int, int> input{readRawInput()};
      switch (std::get<0>(input)) {
      case 't':
      case '\t':
      case 'q':
      case '\033':
        return {};
      case '\n':
      case specKey::leftClickRelease:
        return tocData.data()[selectedNavPointIndex].second;
      case 'h':
      case 'b':
      case ctrlB:
      case specKey::arrowLeft:
      case specKey::pgUp:
        if (selectedNavPointIndex != 0) {
          selectedNavPointIndex -= screenRows / 2;
          selectedNavPointIndex = std::max(selectedNavPointIndex, 0);
          goto redraw_screen;
        }
        break;
      case 'l':
      case 'f':
      case ctrlF:
      case ' ':
      case specKey::arrowRight:
      case specKey::pgDown:
        if (selectedNavPointIndex != std::ssize(tocData) - 1) {
          selectedNavPointIndex += screenRows / 2;
          selectedNavPointIndex = std::min(
              selectedNavPointIndex, static_cast<int>(tocData.size()) - 1);
          goto redraw_screen;
        }
        break;
      case 'u':
      case ctrlU:
        if (selectedNavPointIndex != 0) {
          selectedNavPointIndex -= screenRows / 4;
          selectedNavPointIndex = std::max(selectedNavPointIndex, 0);
          goto redraw_screen;
        }
        break;
      case 'd':
      case ctrlD:
        if (selectedNavPointIndex != std::ssize(tocData) - 1) {
          selectedNavPointIndex += screenRows / 4;
          selectedNavPointIndex = std::min(
              selectedNavPointIndex, static_cast<int>(tocData.size()) - 1);
          goto redraw_screen;
        }
        break;
      case 'k':
      case specKey::arrowUp:
      case specKey::wheelUp:
        if (selectedNavPointIndex != 0) {
          --selectedNavPointIndex;
          goto redraw_screen;
        }
        break;
      case 'j':
      case specKey::arrowDown:
      case specKey::wheelDown:
        if (selectedNavPointIndex != std::ssize(tocData) - 1) {
          ++selectedNavPointIndex;
          goto redraw_screen;
        }
        break;
      case 'g':
      case specKey::home:
        if (selectedNavPointIndex != 0) {
          selectedNavPointIndex = 0;
          goto redraw_screen;
        }
        break;
      case 'G':
      case specKey::end:
        if (selectedNavPointIndex != std::ssize(tocData) - 1) {
          selectedNavPointIndex = static_cast<int>(tocData.size() - 1);
          goto redraw_screen;
        }
        break;
      case specKey::winResize:
        setUpDisplayTOC(tocData, desiredMaxLen, winInfo, tocStr, tocLines,
                        screenRows);
        goto redraw_screen;
      default:
        break;
      }
    }
redraw_screen:
  }
}

auto setUpDisplayTOC(const TocData& tocData, int desiredMaxLen,
                     winsize& winInfo, std::string& tocStr, int& tocLines,
                     int& screenRows) -> void {
  ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);
  screenRows = std::max(static_cast<int>(winInfo.ws_row) - 1, 1);

  tocStr.clear();
  tocDataToString(tocData, tocStr);
  const int maxLen{std::min(desiredMaxLen, static_cast<int>(winInfo.ws_col)
                                               - horizontalMarginChars * 2)};
  processContentText(tocStr, maxLen);

  tocLines = getOccurrences<std::string_view>(tocStr, "\n");
}

auto displayEpub(const EpubProg& iniProg, const fs::path& epubRootAbs,
                 int desiredMaxLen) -> EpubProg {
  const fs::path opfAbs{epubRootAbs / getOPFRel(epubRootAbs)};
  XMLDocument opf{};
  opf.LoadFile(opfAbs.c_str());
  if (opf.Error()) {
    throw std::runtime_error{opf.ErrorStr()};
  }
  const XMLElement* const metadata{getMetadata(opf)};
  const std::string title{getTitle(metadata)};

  std::vector spineWithAbs{getSpine(opf)};
  for (auto& rel : spineWithAbs) {
    rel = opfAbs.parent_path() / rel;
  }

  TocData tocDataWithAbs{getTOC(spineWithAbs[0])};
  for (auto& pair : tocDataWithAbs) {
    pair.second = spineWithAbs[0].parent_path() / pair.second;
  }

  std::size_t spineIndex{1};
  bool found{false};
  while (spineIndex < spineWithAbs.size()) {
    if (spineWithAbs[spineIndex] == iniProg.chapterAbs) {
      found = true;
      break;
    }
    ++spineIndex;
  }
  if (!found) {
    if (iniProg.chapterAbs.empty()) {
      spineIndex = 1;
    }
    else {
      throw std::runtime_error{
          "indicated epub progress chapter not found in spine"};
    }
  }

  double chapterProg{iniProg.chapterProg};

  std::cout << esc << hideCursor;
  std::cout << esc << clearScreen;

  while (true) {
    const std::pair chapterOut{displayChapter(spineWithAbs[spineIndex], title,
                                              chapterProg, desiredMaxLen)};
    switch (chapterOut.first) {
    case ChapterExit::prev:
      if (spineIndex != 1) {
        --spineIndex;
        chapterProg = 1;
      }
      else {
        chapterProg = 0;
      }
      break;
    case ChapterExit::next:
      if (spineIndex != spineWithAbs.size() - 1) {
        ++spineIndex;
        chapterProg = 0;
      }
      else {
        chapterProg = 1;
      }
      break;
    case ChapterExit::toc: {
      int iniNavPointIndex{0};
      for (int i{static_cast<int>(spineIndex)}; i >= 1; --i) {
        for (int j{0}; j < std::ssize(tocDataWithAbs); ++j) {
          if (spineWithAbs.data()[i].lexically_normal()
              == tocDataWithAbs.data()[j].second.lexically_normal()) {
            iniNavPointIndex = j;
            goto exit_nested_loops;
          }
        }
      }
exit_nested_loops:
      const fs::path tocOut{
          displayTOC(tocDataWithAbs, title, desiredMaxLen, iniNavPointIndex)};
      bool found{false};
      for (int i{1}; i < std::ssize(spineWithAbs); ++i) {
        if (spineWithAbs.data()[i].lexically_normal()
            == tocOut.lexically_normal()) {
          found = true;
          spineIndex = static_cast<std::size_t>(i);
          chapterProg = 0;
          break;
        }
      }
      if (!found) {
        chapterProg = chapterOut.second;
      }
      break;
    }
    case ChapterExit::quit:
      eraseScreen();
      std::cout << esc << showCursor;
      return {.chapterAbs = spineWithAbs[spineIndex],
              .chapterProg = chapterOut.second};
    }
  }
}
