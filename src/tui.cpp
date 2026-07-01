#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include "base64.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.hpp"
#include "row_col_diacritics.hpp"
#include "tui.hpp"

namespace fs = std::filesystem;

constexpr std::string esc {'\033'};
constexpr std::string escEnd {esc + '\\'};

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
                                         int channels, int xPixels,
                                         int yPixels, int rows, int cols) {
    std::string ctrlData {""};
    ctrlData += "f=" + std::to_string(channels * 8) + ',';
    ctrlData += "s=" + std::to_string(xPixels) + ',';
    ctrlData += "v=" + std::to_string(yPixels) + ',';
    ctrlData += "r=" + std::to_string(rows) + ',';
    ctrlData += "c=" + std::to_string(cols) + ',';
    ctrlData += "t=t,";
    ctrlData += "i=1,";
    ctrlData += "U=1,";
    ctrlData += "a=T,";
    ctrlData += "q=2";

    const std::string tempDataFileAbsEncoded 
            {base64::to_base64(tempDataFileAbs.string())};

    return esc + "_G" + ctrlData + ';' + tempDataFileAbsEncoded + escEnd;
}

void loadImg(const fs::path& imgAbs, int rows, int cols) {
    int xPixels {};
    int yPixels {};
    int channels {};
    constexpr int noRequiredChannelNum {0};

    unsigned char* pixelData {stbi_load(imgAbs.c_str(),
            &xPixels, &yPixels, &channels, noRequiredChannelNum)};
    if (!pixelData) {
        throw std::runtime_error {"unable to decode image pixel data"};
    }

    const int pixelDataSize {xPixels * yPixels * channels};
    const std::string_view pixelDataView {
            reinterpret_cast<const char*>(pixelData),
            static_cast<std::size_t>(pixelDataSize)};

    const fs::path tempDataFileAbs 
            {"/dev/shm/mnc-img-data-tty-graphics-protocol"};
    std::ofstream tempDataFile {tempDataFileAbs};
    if (!tempDataFile.is_open()) {
        throw std::runtime_error {"temp image data file failed to open"};
    }

    tempDataFile << pixelDataView;
    tempDataFile.close();
    stbi_image_free(pixelData);

    std::string graphicsEscCode {getGraphicsEscCode(
            tempDataFileAbs, channels, xPixels, yPixels, rows, cols)};
    wrapForTmuxPassthrough(graphicsEscCode);
    std::cout << graphicsEscCode;
}

void displayLoadedImg(int rows, int cols) {
    const std::string idInfoInFGColor {esc + "[38;5;" + '1' + 'm'};
    const std::string resetFGColor {esc + "[39m"};
    const std::string placeholderChar {"\U0010EEEE"};

    for (int curRow {0}; curRow < rows; ++curRow) {
        std::string placeholders {""};
        for (int curCol {0}; curCol < cols; ++curCol) {
            placeholders += placeholderChar + rowColDiacritics.data()[curRow] 
                                            + rowColDiacritics.data()[curCol];
        }

        std::cout << idInfoInFGColor << placeholders << resetFGColor << '\n';
    }
}
