#pragma once

#include "base64/base64.hpp"
#include "epub_parser.hpp"
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <tuple>
#include <utility>
#include <vector>

using Key = std::int16_t;

namespace specKey {
enum Values : Key { // NOLINT(cppcoreguidelines-use-enum-class)
  // keyboard keys represented by escape sequences
  arrowLeft = 1000, // don't conflict with normal 8-bit char values
  arrowRight,
  arrowUp,
  arrowDown,
  home,
  end,
  pgUp,
  pgDown,
  // mouse events
  leftClickRelease,
  rightClickRelease,
  wheelUp,
  wheelDown,
  // signals
  winResize,
  // not recognized/supported
  unknown,
};
}

enum class ChapterExit : std::uint8_t {
  prev,
  next,
  quit,
  toc,
};

struct EpubProg {
  std::filesystem::path chapterAbs{};
  double chapterProg{};
};

struct ChapterReadingStats {
  int words{};
  int cjkCharacters{};
};

struct SearchTextSource {
  std::size_t begin{};
  std::size_t end{};
  int line{};
};

struct ChapterSearchIndex {
  std::string text{};
  std::vector<SearchTextSource> source{};
};

struct ChapterSearchMatch {
  std::size_t begin{};
  std::size_t end{};
  int line{};
};

inline constexpr Key ctrlB{2};
inline constexpr Key ctrlF{6};
inline constexpr Key ctrlU{21};
inline constexpr Key ctrlD{4};
inline constexpr Key ctrlW{23};
inline const std::string esc{'\033'};
inline const std::string imgCellPlaceholder{"\U0010EEEE"};
// Internal layout markers. These must be removed before terminal output.
inline const std::string centerAlignBegin{'\x1C'};
inline const std::string centerAlignEnd{'\x1D'};
inline const std::string rightAlignBegin{'\x1E'};
inline const std::string rightAlignEnd{'\x1F'};
inline const std::string forcedWrapMarker{'\x1A'};
// Don't forget to modify `getInvisEscSeqLen()` when you change constants
// below.
inline const std::string escEnd{esc + '\\'};
inline const std::string clearScreen{"[2J"};
inline const std::string posCursorTopLeft{"[H"};
inline const std::string eraseLine{"[2K"};
inline const std::string hideCursor{"[?25l"};
inline const std::string showCursor{"[?25h"};
inline const std::string enableMouseEventReporting{"[?1003h"};
inline const std::string disableMouseEventReporting{"[?1003l"};
inline const std::string enableDecimalReportingFormat{"[?1006h"};
inline const std::string disableDecimalReportingFormat{"[?1006l"};
inline const std::string bold{"[1m"};
inline const std::string resetBold{"[22m"};
inline const std::string italic{"[3m"};
inline const std::string resetItalic{"[23m"};
inline const std::string underline{"[4m"};
inline const std::string resetUnderline{"[24m"};
inline const std::string yellowFG{"[33m"};
inline const std::string redFG{"[31m"};
inline const std::string greenFG{"[32m"};
inline const std::string blueFG{"[34m"};
inline const std::string magentaFG{"[35m"};
inline const std::string cyanFG{"[36m"};
inline const std::string lightGrayFG{"[37m"};
inline const std::string resetFG{"[39m"};
inline const std::string grayBG{"[100m"};
inline const std::string resetBG{"[49m"};

// Load an image to the terminal (create a virtual placement)
// to be displayed later using special unicode characters.
// Image will be shrunk or enlarged, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters.
// `id` must be an integer between 1 and 2^32 - 1, inclusive.
// Must be able to output to stdout through `std::cout` to load image data.
// Throws `std::runtime_error` if:
// `id` is not in valid range,
// `imgAbs` could not be decoded into pixel data,
// or the temp image data file failed to open.
auto loadImg(const std::filesystem::path& imgAbs, std::uint32_t id, int rows,
             int cols) -> void;

// Display a loaded image (existing virtual placement)
// using `rows` * `cols` special unicode characters.
// Appends unicode characters (the image) to `out`.
// `id` must be an integer between 1 and 2^24 - 1, inclusive.
// `forITerm2` enables its more explicit placeholder encoding workaround.
// Throws `std::runtime_error` if `id` is not in valid range.
auto displayLoadedImg(std::uint32_t id, int rows, int cols, std::string& out,
                      bool forITerm2) -> void;

// Using the kitty graphics protocol, display an image to the terminal.
// Appends unicode characters showing the image to `out`.
// If `rows` and `cols` are not provided, image will be displayed
// at its original size, with its original aspect ratio,
// using the minimum amount of characters possible
// (unless the image is larger than window size - hardcoded margin size,
// in which case it will be shrunk down to fit, maintaining its aspect ratio).
// Else, image will be shrunk or enlarged, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters.
// Requires `set -g allow-passthrough on` in `~/.tmux.conf` to work.
// Must be able to output to stdout through `std::cout` to send terminal data.
// Throws `std::runtime_error` for bad `imgAbs`.
// Note: there's a bug on ghostty's end that images displayed on tmux panes on
// the right side of the screen will be broken until tmux redraws the screen.
auto displayImg(const std::filesystem::path& imgAbs, std::string& out,
                int rows = 0, int cols = 0) -> void;

// Constructs the appropriate graphics escape code of the kitty image protocol
// to load an image (create a virtual placement) based on provided parameters.
[[nodiscard]] auto
getGraphicsEscCode(const std::filesystem::path& tempDataFileAbs, int channels,
                   int xPixels, int yPixels, std::uint32_t id, int rows,
                   int cols) -> std::string;

// Trivial function to check if `fd` points to an interactive terminal.
[[nodiscard]] auto isTerm(FILE* fd) -> bool;

// If `fd` refers to a terminal, emits bold + `fgColor` escape codes to
// `std::cout` (when `fd` is `stdout`) or `std::cerr` (otherwise).
// Pair with `resetBoldColorIfTerm(fd)`.
auto boldColorIfTerm(FILE* fd, std::string_view fgColor) -> void;

// If `fd` refers to a terminal, emits reset-bold + reset-fg escape codes
// to the same stream chosen by the matching `boldColorIfTerm` call.
auto resetBoldColorIfTerm(FILE* fd) -> void;

// Restore original terminal settings.
// Depends on `enableRawMode()` to retrieve original terminal setting flags.
// Never call this function before calling `enableRawMode()`.
// Throws `std::runtime_error` on failure to set terminal settings.
auto disableRawMode() -> void;

// Sets terminal to raw mode, disabling echo and canonical mode.
// `read()` returns 0 every 100ms when not receiving input.
// Enables mouse event reporting and registers SIGWINCH handler.
// Sets `disableRawMode()` to be called at program exit.
// Has no effect on repeated calls.
// Throws `std::runtime_error` on failure to read or set terminal settings,
// and on failure to register `disableRawMode()` to run at exit.
auto enableRawMode() -> void;

// Converts a utf-8 encoded string into a wide string (utf-32 on posix).
// Throws `std::system_error` on failure.
[[nodiscard]] auto utf8ToWide(std::string_view input) -> std::wstring;

// Converts a wide string (utf-32 on posix) to a utf-8 encoded string.
// Throws `std::system_error` on failure.
[[nodiscard]] auto wideToUTF8(std::wstring_view input) -> std::string;

// Queries `wcwidth()` for visual length (columns) of `str`.
// `useSystemLocale()` should be called before using this function.
// Overestimates length of `str` containing escape sequences not accounted for
// by `getInvisEscSeqLen()`.
[[nodiscard]] auto getVisualLen(std::wstring_view str) -> int;

// Get length of invisible escape sequence chars in `str`.
// Only looks for plausible escape sequences.
[[nodiscard]] auto getInvisEscSeqLen(std::wstring_view str) -> int;

// Sets program's locale to system locale, updating `std::cout` and `std::cin`.
// Should be called before functions which depend on correct locale.
auto useSystemLocale() -> void;

// Reduce each run of newlines separated by spaces, tabs, non-breaking spaces,
// or zero-width non-joiners to at most two, removing separators between them.
auto collapseConsecutiveNewlines(std::string& str) -> void;

// Split lines with visual length longer than `maxLen` in `str` at spaces,
// or force a split at `maxLen` when no space is available.
// Image lines are left as is to support images wider than `maxLen`.
// This should be the first text content manipulation function called,
// as most others depend on a correct `maxLen`.
auto wrapLines(std::string& str, int maxLen, bool markForcedWraps = false)
    -> void;

// Based on screen width, center text using `maxLen`, images using image width.
// Note this will create lines longer than `maxLen`,
// so it should be one of the last text content manipulation functions called.
auto centerOnScreen(std::string& str, int maxLen) -> void;

// In `str`, using `maxLen`, center justify text beginning with `prefix`
// and ending with `postfix`.
auto centerJustify(std::string_view prefix, std::string_view postfix,
                   std::string& str, int maxLen) -> void;

// In `str`, using `maxLen`, right justify text beginning with `prefix`
// and ending with `postfix`.
auto rightJustify(std::string_view prefix, std::string_view postfix,
                  std::string& str, int maxLen) -> void;

// Read one input in raw mode.
// Returns the key (normal keypress integer values + values in specKey::Values)
// and the row, col position where the action happened, if applicable.
// Throws `std::system_error` on error to read key.
[[nodiscard]] auto readRawInput() -> std::tuple<Key, int, int>;

// Equivalent to clearing the screen, removing its content from the scrollback
// buffer, and setting cursor position to the screen's top left cell.
auto eraseScreen() -> void;

// Get index of nth occurrence of `target` in `str`, starting the search
// from `startIndex`, doesn't count overlapping `target` occurrences.
// Returns `std::string_view::npos` if nth occurrence does not exist.
[[nodiscard]] constexpr auto findNth(std::string_view str,
                                     std::string_view target, int n,
                                     std::size_t startIndex = 0)
    -> std::size_t {
  if (n == 0 || target.empty()) {
    return std::string_view::npos;
  }

  std::size_t targetBeginIndex{startIndex};
  int count{0};

  while ((targetBeginIndex = str.find(target, targetBeginIndex))
         != std::string_view::npos) {
    ++count;
    if (count == n) {
      return targetBeginIndex;
    }
    targetBeginIndex += target.size();
  }
  return std::string_view::npos;
}

// In `str`, replace all occurrences of `target` with `replacement`.
constexpr auto findAndReplaceAll(std::string& str, std::string_view target,
                                 std::string_view replacement) -> void {
  std::size_t pos{str.find(target)};
  while (pos != std::string::npos) {
    str.replace(pos, target.size(), replacement);
    pos = str.find(target, pos + replacement.size());
  }
}

// Modify `str` to wrap its content in tmux's passthrough escape sequence,
// letting escape sequences tmux doesn't know about reach the terminal
// emulator. Requires `set -g allow-passthrough on` in `~/.tmux.conf` for
// passthrough.
constexpr auto wrapForTmuxPassthrough(std::string& str) -> void {
  findAndReplaceAll(str, esc, esc + esc);
  str = esc + "Ptmux;" + str + escEnd;
}

// Get number of occurrences of `target` in `str`.
// Overlapping `target` occurrences are not counted.
template <typename TStrView>
[[nodiscard]] constexpr auto getOccurrences(TStrView str, TStrView target)
    -> int {
  int count{0};
  std::size_t pos{};
  while ((pos = str.find(target, pos)) != TStrView::npos) {
    ++count;
    pos += target.size();
  }
  return count;
}

// In `str`, if `style` and `resetStyle` encompass multiple lines,
// give each line its own `style` and `resetStyle`.
auto styleEachLineIndividually(std::string& str, std::string_view style,
                               std::string_view resetStyle) -> void;

// Process content text of epubs extracted from chapter xhtml files for
// display.
auto processContentText(std::string& str, int maxLen,
                        bool markForcedWraps = false) -> void;

// Build normalized, ASCII-case-folded visible text and retain its mapping to
// the rendered chapter. ANSI escapes, images, and artificial whitespace are
// not searchable.
[[nodiscard]] auto buildChapterSearchIndex(std::string_view chapter)
    -> ChapterSearchIndex;

// Find non-overlapping occurrences of `query` in a chapter search index.
[[nodiscard]] auto findChapterSearchMatches(const ChapterSearchIndex& index,
                                            std::string_view query)
    -> std::vector<ChapterSearchMatch>;

// Return the displayed chapter slice with all matches highlighted in gray.
[[nodiscard]] auto highlightSearchMatches(
    std::string_view chapter, const ChapterSearchIndex& index,
    const std::vector<ChapterSearchMatch>& matches, std::size_t displayBegin,
    std::size_t displayEnd) -> std::string;

// Return the one-based terminal row and column for a visible match's first
// character, accounting for ANSI escapes and wide characters.
[[nodiscard]] auto getSearchMatchCursorPosition(std::string_view chapter,
                                                const ChapterSearchIndex& index,
                                                const ChapterSearchMatch& match,
                                                int screenTopLine,
                                                int screenBotLine)
    -> std::optional<std::pair<int, int>>;

// Remove the final UTF-8 code point, including any trailing continuation
// bytes. Invalid trailing bytes are removed one at a time.
auto popLastUTF8CodePoint(std::string& str) -> void;

// Remove trailing whitespace and the final whitespace-delimited word.
auto popLastSearchWord(std::string& str) -> void;

// Append a complete printable UTF-8 input character to `query`. Incomplete
// multibyte input is retained in `pending`; controls and malformed input are
// rejected. Returns whether `query` changed.
auto appendSearchInputByte(std::string& query, std::string& pending,
                           unsigned char byte) -> bool;

// Count words and CJK characters in processed chapter text, ignoring terminal
// escapes and combining marks.
[[nodiscard]] auto getChapterReadingStats(std::string_view chapter)
    -> ChapterReadingStats;

// Build a full-width status line for the chapter's current viewport.
[[nodiscard]] auto getChapterProgressIndicator(
    int screenTopLine, int screenRows, int screenCols, int chapterLines,
    const ChapterReadingStats& chapterReadingStats, std::string_view title,
    std::string_view searchQuery = {}, std::string_view searchMatchInfo = {},
    int* searchCursorCol = nullptr) -> std::string;

// In raw mode, create a tui interface to view `chapterAbs`.
// Chapter displayed starting from `iniProg`, lines wrapped at `desiredMaxLen`.
// Returns reason for exit and progress at exit.
[[nodiscard]] auto displayChapter(const std::filesystem::path& chapterAbs,
                                  std::string_view title, double iniProg,
                                  int desiredMaxLen)
    -> std::pair<ChapterExit, double>;

// Helper for `displayChapter()`.
auto setUpDisplayChapter(const std::filesystem::path& chapterAbs, double prog,
                         int desiredMaxLen, winsize& winInfo,
                         std::string& chapter, int& chapterLines,
                         int& screenRows, int& screenTopLine,
                         int& screenBotLine) -> void;

// Helper for `displayChapter()`.
auto snapTopLineToBound(int& screenTopLine) -> void;

// Helper for `displayChapter()`.
auto snapBotLineToBound(int& screenBotLine, int chapterLines) -> void;

// Helper for `displayChapter()`.
[[nodiscard]] auto calcBotLineFromTopLine(int screenTopLine, int screenRows)
    -> int;

// Helper for `displayChapter()`.
[[nodiscard]] auto calcTopLineFromBotLine(int screenBotLine, int screenRows)
    -> int;

// Execute a command using `posix_spawnp()`, waiting until the command exits.
// `argV` first element should be the command binary name,
// following elements should be the individual arguments.
// Returns what the command printed to stdout.
// Throws `std::invalid_argument` for empty command binary name.
// Throws `std::system_error` on error creating pipe, setting up spawn file
// actions, spawning, reading command output, or waiting for command.
// Throws `std::runtime_error` for an abnormal exit from command.
auto execute(const std::vector<std::string>& argV) -> std::string;

// Sets `g_winResize` to 1.
extern "C" auto handleSigwinch([[maybe_unused]] int signal) -> void;

// Start listening for SIGWINCH signals, setting `g_winResize` to 1 on receive.
// Throws `std::runtime_error` on failure to register handler via
// `sigaction()`.
auto registerSigwinchHandler() -> void;

// Translate `data` into a chapter-like string suitable for display.
// Output is appended to `str`.
auto tocDataToString(const TocData& data, std::string& str) -> void;

// Build a full-width table of contents status line.
[[nodiscard]] auto getTOCStatusLine(int screenCols, std::string_view title)
    -> std::string;

// Checks `TMUX`, falling back to `TERM_PROGRAM`, for whether or not running in
// a tmux session.
[[nodiscard]] auto inTmuxSession() -> bool;

// Checks `TERM_PROGRAM`, falling back to `LC_TERMINAL` for tmux sessions, for
// whether or not running in iTerm2.
[[nodiscard]] auto inITerm2Session() -> bool;

// In raw mode, create a tui interface to view `tocData`.
// Lines wrapped at `desiredMaxLen`.
// Provide initial selected chapter through `selectedNavPointIndex`.
// Returns relative path found in `tocData` of selected chapter,
// or an empty path if user exited without selecting one.
// Throws `std::logic_error` if provided nav point index is out of bounds.
[[nodiscard]] auto displayTOC(const TocData& tocData, std::string_view title,
                              int desiredMaxLen, int selectedNavPointIndex)
    -> std::filesystem::path;

// Helper for `displayTOC()`.
auto setUpDisplayTOC(const TocData& tocData, int desiredMaxLen,
                     winsize& winInfo, std::string& tocStr, int& tocLines,
                     int& screenRows) -> void;

// Highest level function for creating the core TUI interface.
// Once in raw mode, with locale set, display an epub book.
// Takes an unzipped epub, desired visual length, and initial progress.
// If `iniProg.chapterAbs` is empty, it will be taken as the first chapter.
// Returns progress at exit from this function.
// Throws `std::runtime_error` for bad initial progress chapter.
[[nodiscard]] auto displayEpub(const EpubProg& iniProg,
                               const std::filesystem::path& epubRootAbs,
                               int desiredMaxLen) -> EpubProg;
