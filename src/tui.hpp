#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include "base64.hpp"

namespace fs = std::filesystem;

inline constexpr std::string esc {'\033'};
inline constexpr std::string escEnd {esc + '\\'};
inline constexpr std::string clearScreen {"[2J"};
inline constexpr std::string posCursorTopLeft {"[H"};
inline constexpr std::string eraseLine {"[2K"};
inline constexpr std::string bold {"[1m"};
inline constexpr std::string resetBold {"[22m"};
inline constexpr std::string italic {"[3m"};
inline constexpr std::string resetItalic {"[23m"};
inline constexpr std::string yellowFG {"[33m"};
inline constexpr std::string redFG {"[31m"};
inline constexpr std::string resetFG {"[39m"};

namespace key {
    enum keyValues : int {
        arrowLeft = 1000,
        arrowRight,
        arrowUp,
        arrowDown,
        home,
        end,
        pgUp,
        pgDown,
        unknownEscSeq,
    };
}

// load an image to the terminal (create a virtual placement)
// to be displayed later using special unicode characters;
// image will be shrunk or enlargened, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters;
// `id` must be an integer between 1 and 2^32 - 1, inclusive;
// note: the function must be able to output to and flush stdout through `cout`;
// throws `std::runtime_error` if `id` is not in valid range,
// `imgAbs` could not be decoded into pixel data,
// or the temp image data shared memory file failed to open
void loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols);

// display a loaded image (existing virtual placement) 
// using `rows` * `cols` special unicode characters;
// appends unicode characters (the image) to `out`;
// `id` must be an integer between 1 and 2^24 - 1, inclusive;
// throws `std::runtime_error` if `id` is not in valid range
void displayLoadedImg(std::uint32_t id, int rows, int cols, std::string &out);

// using the kitty graphics protocol, display an image to the terminal;
// appends unicode characters (the image) to `out`;
// if `rows` and `cols` are not provided, image will be displayed
// at its original size, with its original aspect ratio,
// using the minimum amount of characters possible
// (unless the image is larger than the window size, in which case it will be
// shrunk down to fit the window, maintaining its aspect ratio);
// else, image will be shrunk or enlargened, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters;
// note: may require `set -g allow-passthrough on` in ~/.tmux.conf to work;
// throws `std::runtime_error` for bad `imgAbs`
void displayImg(const fs::path& imgAbs, std::string& out,
                int rows = 0, int cols = 0);

// in `str`, replace all occurences of `target` with `replacement`
constexpr void findAndReplaceAll(std::string& str, std::string_view target, 
                                 std::string_view replacement) {
    std::size_t pos = str.find(target);
    while (pos != std::string::npos) {
        str.replace(pos, target.size(), replacement);
        pos = str.find(target, pos + replacement.size());
    }
}

// modify `str` to wrap it in tmux's passthrough escape sequence,
// letting escape sequences tmux doesn't understand reach the terminal emulator;
// may require `set -g allow-passthrough on` in ~/.tmux.conf to work
constexpr void wrapForTmuxPassthrough(std::string& str) {
    findAndReplaceAll(str, esc, esc + esc);
    str = esc + "Ptmux;" + str + escEnd;
}

// constructs the appropriate graphics escape code of the kitty image protocol
// to load an image (create a virtual placement) based on provided parameters
constexpr std::string getGraphicsEscCode(const fs::path& tempDataFileAbs,
                                         int channels, int xPixels, int yPixels,
                                         std::uint32_t id, int rows, int cols) {
    std::string ctrlData {""};
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

    const std::string tempDataFileAbsEncoded 
            {base64::to_base64(tempDataFileAbs.string())};

    return esc + "_G" + ctrlData + ';' + tempDataFileAbsEncoded + escEnd;
}

// resets terminal settings to original flags;
// depends on enableRawMode() to retrieve original settings first,
// never call this function before calling enableRawMode();
// throws `std::runtime_error` on failure to set terminal settings
void disableRawMode();

// sets terminal to raw mode, disabling echo and canonical mode;
// read() returns 0 every 100ms when not receiving input;
// sets disableRawMode() to be called at program exit;
// throws `std::runtime_error` on failure to read or set terminal settings
void enableRawMode();

// converts a utf-8 encoded string into a wide string (utf-32 on posix);
// throws `std::system_error` on failure
std::wstring utf8ToWide(std::string_view input);

// converts a wide string (utf-32 on posix) to a utf-8 encoded string;
// throws `std::system_error` on failure
std::string wideToUTF8(std::wstring_view input);

// queries wcwidth() for visual length (columns) of `str`;
// useSystemLocale() should be called beforehand;
// overestimates length of string containing escape sequences not accounted for
// by getInvisEscSeqLen()
int getVisualLen(std::wstring_view str);

// get number of occurences of `target` in `str`
template <typename TStrView>
constexpr int getOccurences(TStrView str, TStrView target) {
    int count {0};
    std::size_t pos {};
    while ((pos = str.find(target, pos)) != std::string::npos) {
        ++count;
        pos += target.size();
    }
    return count;
}

// get length of invisible escape sequence chars in `str`, 
// only looks for plausible escape sequences
constexpr int getInvisEscSeqLen(std::wstring_view str) {
    int count {0};
    count += getOccurences<std::wstring_view>(str, L"\033[1m") * 3;
    count += getOccurences<std::wstring_view>(str, L"\033[22m") * 4;
    count += getOccurences<std::wstring_view>(str, L"\033[3m") * 3;
    count += getOccurences<std::wstring_view>(str, L"\033[23m") * 4;
    count += getOccurences<std::wstring_view>(str, L"\033[33m") * 4;
    count += getOccurences<std::wstring_view>(str, L"\033[31m") * 4;
    count += getOccurences<std::wstring_view>(str, L"\033[39m") * 4;
    return count;
}

// sets program's locale to system locale, updating `std::cout` and `std::cin`;
// should be one of the first functions the program calls
void useSystemLocale();

// split lines with visual length longer than `maxLen` in `str` at spaces;
// if a space is not encountered on a long line, it is left as is
// (this is to support displaying images wider than `maxLen`);
// this should be the first text content manipulation function called
void wrapLines(std::string& str, int maxLen);

// based on screen width, center text using `maxLen`, images using img width;
// this will create lines longer than `maxLen`, 
// so it should be the last text content manipulation function called
void centerContentOnScreen(std::string& str, int maxLen);

// in `str`, using `maxLen`, center justify text beginning with `prefix` 
// and ending with `postfix`
void centerJustify(std::string_view prefix, std::string_view postfix, 
                   std::string& str, int maxLen);

// read one key input in raw mode;
// for normal keypresses, returns the character promoted to an int; 
// for those represented by escape seqs, returns a value in key::keyValues;
// throws `std::system_error` on error to read key
int rawReadKey();

// equivalent to clearing the screen but not putting it in scrollback buffer
void eraseScreen();
