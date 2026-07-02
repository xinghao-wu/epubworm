#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include "base64.hpp"

namespace fs = std::filesystem;

inline constexpr std::string esc {'\033'};
inline constexpr std::string escEnd {esc + '\\'};

void loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols);

void displayLoadedImg(std::uint32_t id, int rows, int cols);

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

constexpr void wrapForTmuxPassthrough(std::string& str) {
    findAndReplaceAll(str, esc, esc + esc);
    str = esc + "Ptmux;" + str + escEnd;
}

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
