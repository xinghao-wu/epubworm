#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include "base64.hpp"

namespace fs = std::filesystem;

inline constexpr std::string esc {'\033'};
inline constexpr std::string escEnd {esc + '\\'};

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
// helper to enableRawMode(), never call this function without calling
// enableRawMode() first;
// throws `std::runtime_error` on failure to set terminal settings
void disableRawMode();

// sets terminal to raw mode, disabling echo and canonical mode;
// sets disableRawMode() to be called at program exit;
// throws `std::runtime_error` on failure to read or set terminal settings
void enableRawMode();
