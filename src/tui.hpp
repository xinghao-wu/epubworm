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
// to be displayed later using unicode placeholder chars;
// `id` must be an integer between 1 and 2^32 - 1, inclusive;
// see `displayImg()` for documentation of other parameters
void loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols);

// display a loaded image (virtual placement) using unicode placeholder chars;
// `id` must be an integer between 1 and 2^24 - 1, inclusive;
// see `displayImg()` for documentation of other parameters
void displayLoadedImg(std::uint32_t id, int rows, int cols);

// using the kitty graphics protocol, display an image to the terminal;
// prints `rows` * `cols` characters to display the image;
// image will be shrunk or enlargened, maintaining its aspect ratio,
// to fit centered within `rows` * `cols` characters;
// note: this function's interface will likely change to not need
// the `rows` and `cols` parameters once I figure out what method to use to 
// query the terminal for screen size in characters and character size in pixels
void displayImg(const fs::path& imgAbs, int rows, int cols);

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
// requires `set -g allow-passthrough on` in ~/.tmux.conf to work
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
