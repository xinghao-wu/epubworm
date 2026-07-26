#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <csignal>
#include "base64.hpp"

namespace fs = std::filesystem;

extern volatile std::sig_atomic_t g_winResize;

inline constexpr std::string esc {'\033'};
inline constexpr std::string escEnd {esc + '\\'};
inline constexpr std::string clearScreen {"[2J"};
inline constexpr std::string posCursorTopLeft {"[H"};
inline constexpr std::string eraseLine {"[2K"};
inline constexpr std::string hideCursor {"[?25l"};
inline constexpr std::string showCursor {"[?25h"};
inline constexpr std::string enableMouseEventReporting {"[?1003h"};
inline constexpr std::string disableMouseEventReporting {"[?1003l"};
inline constexpr std::string enableDecimalReportingFormat {"[?1006h"};
inline constexpr std::string disableDecimalReportingFormat {"[?1006l"};
inline constexpr std::string bold {"[1m"};
inline constexpr std::string resetBold {"[22m"};
inline constexpr std::string italic {"[3m"};
inline constexpr std::string resetItalic {"[23m"};
inline constexpr std::string yellowFG {"[33m"};
inline constexpr std::string redFG {"[31m"};
inline constexpr std::string resetFG {"[39m"};
inline constexpr std::string imgCellPlaceholder {"\U0010EEEE"};

using Key = std::int16_t;

namespace specKey {
    enum Values : Key {
        // keyboard keys represented by esc seqs
        arrowLeft = 1000, // don't conflict with normal 8 bit char values
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

// load an image to the terminal (create a virtual placement)
// to be displayed later using special unicode characters;
// image will be shrunk or enlargened, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters;
//  `id` must be an integer between 1 and 2^32 - 1, inclusive;
// must be able to output to stdout through `std::cout` to load img data;
// throws `std::runtime_error` if:
//  `id` is not in valid range,
//  `imgAbs` could not be decoded into pixel data,
// or the temp image data shared memory file failed to open
void loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols);

// display a loaded image (existing virtual placement)
// using `rows` * `cols` special unicode characters;
// appends unicode characters (the image) to `out`;
//  `id` must be an integer between 1 and 2^24 - 1, inclusive;
// throws `std::runtime_error` if `id` is not in valid range
void displayLoadedImg(std::uint32_t id, int rows, int cols, std::string &out);

// using the kitty graphics protocol, display an image to the terminal;
// appends unicode characters showing the image to `out`;
// if `rows` and `cols` are not provided, image will be displayed
// at its original size, with its original aspect ratio,
// using the minimum amount of characters possible
// (unless the image is larger than window size - hardcoded margin size,
// in which case it will be shrunk down to fit, maintaining its aspect ratio);
// else, image will be shrunk or enlargened, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters;
// requires `set -g allow-passthrough on` in ~/.tmux.conf to work;
// must be able to output to stdout through `std::cout` to send terminal data;
// throws `std::runtime_error` for bad `imgAbs`;
// note: there's a bug on ghostty's end that images displayed on tmux panes on
// the right side of the screen will be broken until tmux redraws the screen
void displayImg(const fs::path& imgAbs, std::string& out,
                int rows = 0, int cols = 0);

// constructs the appropriate graphics escape code of the kitty image protocol
// to load an image (create a virtual placement) based on provided parameters
std::string getGraphicsEscCode(const fs::path& tempDataFileAbs, int channels,
                               int xPixels, int yPixels, std::uint32_t id,
                               int rows, int cols);

// restore original terminal settings;
// depends on `enableRawMode()` to retrieve original terminal setting flags;
// never call this function before calling `enableRawMode()`;
// throws `std::runtime_error` on failure to set terminal settings
void disableRawMode();

// sets terminal to raw mode, disabling echo and canonical mode;
//  `read()` returns 0 every 100ms when not receiving input;
// enables mouse event reporting and register SIGWINCH handler;
// sets `disableRawMode()` to be called at program exit;
// throws `std::runtime_error` on failure to read or set terminal settings,
// and on failure to register `disableRawMode()` to run at exit
void enableRawMode();

// converts a utf-8 encoded string into a wide string (utf-32 on posix);
// throws `std::system_error` on failure
std::wstring utf8ToWide(std::string_view input);

// converts a wide string (utf-32 on posix) to a utf-8 encoded string;
// throws `std::system_error` on failure
std::string wideToUTF8(std::wstring_view input);

// queries `wcwidth()` for visual length (columns) of `str`;
// `useSystemLocale()` should be called before using this function;
// overestimates length of `str` containing escape sequences not accounted for
// by `getInvisEscSeqLen()`
int getVisualLen(std::wstring_view str);

// get length of invisible escape sequence chars in `str`;
// only looks for plausible escape sequences
int getInvisEscSeqLen(std::wstring_view str);

// sets program's locale to system locale, updating `std::cout` and `std::cin`;
// should be called before functions which depend on correct locale
void useSystemLocale();

// split lines with visual length longer than `maxLen` in `str` at spaces;
// if a space is not encountered on a long line, it is left as is
// (this is to support displaying images wider than `maxLen`);
// this should be the first text content manipulation function called,
// as most others depend on a correct `maxLen`
void wrapLines(std::string& str, int maxLen);

// based on screen width, center text using `maxLen`, images using img width;
// note this will create lines longer than `maxLen`,
// so it should be one of the last text content manipulation functions called
void centerOnScreen(std::string& str, int maxLen);

// in `str`, using `maxLen`, center justify text beginning with `prefix`
// and ending with `postfix`
void centerJustify(std::string_view prefix, std::string_view postfix,
                   std::string& str, int maxLen);

// read one input in raw mode;
// returns the key (normal keypress integer values + values in specKey::Values)
// and the row, col position where the action happened, if applicable;
// throws `std::system_error` on error to read key
std::tuple<Key, int, int> readRawInput();

// equivalent to clearing the screen, removing its content from the scrollback
// buffer, and setting cursor position to the screen's top left cell
void eraseScreen();

// get index of nth occurence of `target` in `str`, starting the search
// from `startIndex`, doesn't count overlapping `target` occurences;
// returns `std::string_view::npos` if nth occurence does not exist
constexpr std::size_t findNth(std::string_view str, std::string_view target,
                              int n, std::size_t startIndex = 0) {
    if (n == 0 || target.empty()) {
        return std::string_view::npos;
    }

    std::size_t targetBeginIndex {startIndex};
    int count {0};

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

// in `str`, replace all occurences of `target` with `replacement`
constexpr void findAndReplaceAll(std::string& str, std::string_view target,
                                 std::string_view replacement) {
    std::size_t pos {str.find(target)};
    while (pos != std::string::npos) {
        str.replace(pos, target.size(), replacement);
        pos = str.find(target, pos + replacement.size());
    }
}

// modify `str` to wrap its content in tmux's passthrough escape sequence,
// letting escape sequences tmux doesn't know abt reach the terminal emulator;
// requires `set -g allow-passthrough on` in ~/.tmux.conf for passthrough
constexpr void wrapForTmuxPassthrough(std::string& str) {
    findAndReplaceAll(str, esc, esc + esc);
    str = esc + "Ptmux;" + str + escEnd;
}

// get number of occurences of `target` in `str`;
// overlapping `target` occurences are not counted
template <typename TStrView>
constexpr int getOccurences(TStrView str, TStrView target) {
    int count {0};
    std::size_t pos {};
    while ((pos = str.find(target, pos)) != TStrView::npos) {
        ++count;
        pos += target.size();
    }
    return count;
}

// process content text of epubs extracted from chapter xhtml files for display
void processContentText(std::string& str, int maxLen);

// in raw mode, create a tui interface to view `chapterAbs`;
// chapter displayed starting from `iniProg`, lines wrapped at `desiredMaxLen`;
// returns reason for exit and progress at exit
std::pair<ChapterExit, double> displayChapter(
        const fs::path& chapterAbs, double iniProg, int desiredMaxLen);

// execute a command using `posix_spawnp()`, waiting until the command exits;
//  `argV` first element should be the command binary name,
// following elements should be the individual arguments;
// throws `std::invalid_argument` for empty command binary name;
// throws `std::system_error` on error spawning or waiting for command;
// throws `std::runtime_error` for an abnormal exit from command
void execute(const std::vector<std::string>& argV);

// sets `g_winResize` to 1
extern "C" void handleSigwinch([[maybe_unused]] int signal);

// start listening for SIGWINCH signals, setting `g_winResize` to 1 on receive;
// throws `std::runtime_error` on failure to register handler via `sigaction()`
void registerSigwinchHandler();
