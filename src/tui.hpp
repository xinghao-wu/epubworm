#pragma once

#include "base64.hpp"
#include "epub_parser.hpp"
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <tuple>
#include <utility>
#include <vector>

using Key = std::int16_t;

namespace specKey {
enum Values : Key {
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

enum class ChapterExit {
    prev,
    next,
    quit,
    toc,
};

struct EpubProg {
    std::filesystem::path chapterAbs{};
    double chapterProg{};
};

extern volatile std::sig_atomic_t g_winResize;

inline constexpr Key ctrlB{2};
inline constexpr Key ctrlF{6};
inline constexpr Key ctrlU{21};
inline constexpr Key ctrlD{4};
inline const std::string esc{'\033'};
inline const std::string imgCellPlaceholder{"\U0010EEEE"};
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
inline const std::string yellowFG{"[33m"};
inline const std::string redFG{"[31m"};
inline const std::string greenFG{"[32m"};
inline const std::string blueFG{"[34m"};
inline const std::string magentaFG{"[35m"};
inline const std::string cyanFG{"[36m"};
inline const std::string resetFG{"[39m"};

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
void loadImg(const std::filesystem::path& imgAbs, std::uint32_t id, int rows,
             int cols);

// Display a loaded image (existing virtual placement)
// using `rows` * `cols` special unicode characters.
// Appends unicode characters (the image) to `out`.
// `id` must be an integer between 1 and 2^24 - 1, inclusive.
// Throws `std::runtime_error` if `id` is not in valid range.
void displayLoadedImg(std::uint32_t id, int rows, int cols, std::string& out);

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
void displayImg(const std::filesystem::path& imgAbs, std::string& out,
                int rows = 0, int cols = 0);

// Constructs the appropriate graphics escape code of the kitty image protocol
// to load an image (create a virtual placement) based on provided parameters.
[[nodiscard]] std::string
getGraphicsEscCode(const std::filesystem::path& tempDataFileAbs, int channels,
                   int xPixels, int yPixels, std::uint32_t id, int rows,
                   int cols);

// Trivial function to check if `fd` points to an interactive terminal.
[[nodiscard]] bool isTerm(FILE* fd);

// If `fd` refers to a terminal, emits bold + `fgColor` escape codes to
// `std::cout` (when `fd` is `stdout`) or `std::cerr` (otherwise).
// Pair with `resetBoldColorIfTerm(fd)`.
void boldColorIfTerm(FILE* fd, std::string_view fgColor);

// If `fd` refers to a terminal, emits reset-bold + reset-fg escape codes
// to the same stream chosen by the matching `boldColorIfTerm` call.
void resetBoldColorIfTerm(FILE* fd);

// Restore original terminal settings.
// Depends on `enableRawMode()` to retrieve original terminal setting flags.
// Never call this function before calling `enableRawMode()`.
// Throws `std::runtime_error` on failure to set terminal settings.
void disableRawMode();

// Sets terminal to raw mode, disabling echo and canonical mode.
// `read()` returns 0 every 100ms when not receiving input.
// Enables mouse event reporting and registers SIGWINCH handler.
// Sets `disableRawMode()` to be called at program exit.
// Has no effect on repeated calls.
// Throws `std::runtime_error` on failure to read or set terminal settings,
// and on failure to register `disableRawMode()` to run at exit.
void enableRawMode();

// Converts a utf-8 encoded string into a wide string (utf-32 on posix).
// Throws `std::system_error` on failure.
[[nodiscard]] std::wstring utf8ToWide(std::string_view input);

// Converts a wide string (utf-32 on posix) to a utf-8 encoded string.
// Throws `std::system_error` on failure.
[[nodiscard]] std::string wideToUTF8(std::wstring_view input);

// Queries `wcwidth()` for visual length (columns) of `str`.
// `useSystemLocale()` should be called before using this function.
// Overestimates length of `str` containing escape sequences not accounted for
// by `getInvisEscSeqLen()`.
[[nodiscard]] int getVisualLen(std::wstring_view str);

// Get length of invisible escape sequence chars in `str`.
// Only looks for plausible escape sequences.
[[nodiscard]] int getInvisEscSeqLen(std::wstring_view str);

// Sets program's locale to system locale, updating `std::cout` and `std::cin`.
// Should be called before functions which depend on correct locale.
void useSystemLocale();

// Reduce each run of newlines separated by spaces, tabs, or non-breaking
// spaces to at most two, removing whitespace between the newlines.
void collapseConsecutiveNewlines(std::string& str);

// Split lines with visual length longer than `maxLen` in `str` at spaces.
// If a space is not encountered on a long line, it is left as is
// (this is to support displaying images wider than `maxLen`).
// This should be the first text content manipulation function called,
// as most others depend on a correct `maxLen`.
void wrapLines(std::string& str, int maxLen);

// Based on screen width, center text using `maxLen`, images using image width.
// Note this will create lines longer than `maxLen`,
// so it should be one of the last text content manipulation functions called.
void centerOnScreen(std::string& str, int maxLen);

// In `str`, using `maxLen`, center justify text beginning with `prefix`
// and ending with `postfix`.
void centerJustify(std::string_view prefix, std::string_view postfix,
                   std::string& str, int maxLen);

// Read one input in raw mode.
// Returns the key (normal keypress integer values + values in specKey::Values)
// and the row, col position where the action happened, if applicable.
// Throws `std::system_error` on error to read key.
[[nodiscard]] std::tuple<Key, int, int> readRawInput();

// Equivalent to clearing the screen, removing its content from the scrollback
// buffer, and setting cursor position to the screen's top left cell.
void eraseScreen();

// Get index of nth occurrence of `target` in `str`, starting the search
// from `startIndex`, doesn't count overlapping `target` occurrences.
// Returns `std::string_view::npos` if nth occurrence does not exist.
[[nodiscard]] constexpr std::size_t findNth(std::string_view str,
                                            std::string_view target, int n,
                                            std::size_t startIndex = 0) {
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
constexpr void findAndReplaceAll(std::string& str, std::string_view target,
                                 std::string_view replacement) {
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
constexpr void wrapForTmuxPassthrough(std::string& str) {
    findAndReplaceAll(str, esc, esc + esc);
    str = esc + "Ptmux;" + str + escEnd;
}

// Get number of occurrences of `target` in `str`.
// Overlapping `target` occurrences are not counted.
template <typename TStrView>
[[nodiscard]] constexpr int getOccurrences(TStrView str, TStrView target) {
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
void styleEachLineIndividually(std::string& str, std::string_view style,
                               std::string_view resetStyle);

// Process content text of epubs extracted from chapter xhtml files for
// display.
void processContentText(std::string& str, int maxLen);

// In raw mode, create a tui interface to view `chapterAbs`.
// Chapter displayed starting from `iniProg`, lines wrapped at `desiredMaxLen`.
// Returns reason for exit and progress at exit.
[[nodiscard]] std::pair<ChapterExit, double>
displayChapter(const std::filesystem::path& chapterAbs, double iniProg,
               int desiredMaxLen);

// Helper for `displayChapter()`.
void setUpDisplayChapter(const std::filesystem::path& chapterAbs, double prog,
                         int desiredMaxLen, winsize& winInfo,
                         std::string& chapter, int& chapterLines,
                         int& screenTopLine, int& screenBotLine);

// Helper for `displayChapter()`.
void snapTopLineToBound(int& screenTopLine);

// Helper for `displayChapter()`.
void snapBotLineToBound(int& screenBotLine, int chapterLines);

// Helper for `displayChapter()`.
[[nodiscard]] int calcBotLineFromTopLine(int screenTopLine,
                                         const winsize& winInfo);

// Helper for `displayChapter()`.
[[nodiscard]] int calcTopLineFromBotLine(int screenBotLine,
                                         const winsize& winInfo);

// Execute a command using `posix_spawnp()`, waiting until the command exits.
// `argV` first element should be the command binary name,
// following elements should be the individual arguments.
// Returns what the command printed to stdout.
// Throws `std::invalid_argument` for empty command binary name.
// Throws `std::system_error` on error creating pipe, setting up spawn file
// actions, spawning, reading command output, or waiting for command.
// Throws `std::runtime_error` for an abnormal exit from command.
std::string execute(const std::vector<std::string>& argV);

// Sets `g_winResize` to 1.
extern "C" void handleSigwinch([[maybe_unused]] int signal);

// Start listening for SIGWINCH signals, setting `g_winResize` to 1 on receive.
// Throws `std::runtime_error` on failure to register handler via
// `sigaction()`.
void registerSigwinchHandler();

// Translate `data`, `title`, and `author` into a chapter-like string suitable
// for display.
// Output is appended to `str`.
void tocDataToString(const TocData& data, std::string_view title,
                     std::string_view author, std::string& str);

// Checks `TMUX`, falling back to `TERM_PROGRAM`, for whether or not running in
// a tmux session.
[[nodiscard]] bool inTmuxSession();

// In raw mode, create a tui interface to view `tocData` with the epub's title
// and author in the header.
// Lines wrapped at `desiredMaxLen`.
// Provide initial selected chapter through `selectedNavPointIndex`.
// Returns relative path found in `tocData` of selected chapter,
// or an empty path if user exited without selecting one.
// Throws `std::logic_error` if provided nav point index is out of bounds.
[[nodiscard]] std::filesystem::path displayTOC(const TocData& tocData,
                                               std::string_view title,
                                               std::string_view author,
                                               int desiredMaxLen,
                                               int selectedNavPointIndex);

// Helper for `displayTOC()`.
void setUpDisplayTOC(const TocData& tocData, std::string_view title,
                     std::string_view author, int desiredMaxLen,
                     winsize& winInfo, std::string& tocStr, int& tocLines);

// Highest level function for creating the core TUI interface.
// Once in raw mode, with locale set, display an epub book.
// Takes an unzipped epub, desired visual length, and initial progress.
// If `iniProg.chapterAbs` is empty, it will be taken as the first chapter.
// Returns progress at exit from this function.
// Throws `std::runtime_error` for bad initial progress chapter.
[[nodiscard]] EpubProg displayEpub(const EpubProg& iniProg,
                                   const std::filesystem::path& epubRootAbs,
                                   int desiredMaxLen);
