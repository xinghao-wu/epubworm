#include <asm-generic/ioctls.h>
#include <random>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <sys/ioctl.h>
#include <thread>
#include <chrono>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.hpp"
#include "row_col_diacritics.hpp"
#include "tui.hpp"

namespace fs = std::filesystem;

void loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols) {
    if (id == 0) {
        throw std::runtime_error("image id not in valid range");
    }

    int xPixels {};
    int yPixels {};
    int channels {};
    constexpr int noRequiredChannelNum {0};

    unsigned char* pixelData {stbi_load(imgAbs.c_str(), &xPixels, &yPixels,
                              &channels, noRequiredChannelNum)};
    if (!pixelData) {
        throw std::runtime_error {stbi_failure_reason()};
    }

    const int pixelDataSize {xPixels * yPixels * channels};
    const std::string_view pixelDataView 
            {reinterpret_cast<const char*>(pixelData),
             static_cast<std::size_t>(pixelDataSize)};

    const fs::path tempDataFileAbs 
            {"/dev/shm/mnc-img-data-tty-graphics-protocol"};
    std::ofstream tempDataFile {tempDataFileAbs};
    if (!tempDataFile.is_open()) {
        throw std::runtime_error {"image temp data file failed to open"};
    }

    tempDataFile << pixelDataView;
    tempDataFile.close();
    stbi_image_free(pixelData);

    std::string graphicsEscCode {getGraphicsEscCode(
            tempDataFileAbs, channels, xPixels, yPixels, id, rows, cols)};
    wrapForTmuxPassthrough(graphicsEscCode);
    std::cout << graphicsEscCode << std::flush;
}

void displayLoadedImg(std::uint32_t id, int rows, int cols, std::string& out) {
    if (id < 1 || id > static_cast<std::uint32_t>((1 << 24) - 1)) {
        throw std::runtime_error("image id not in valid range");
    }

    const std::uint32_t idRed {(id >> 16) & 255};
    const std::uint32_t idGreen {(id >> 8) & 255};
    const std::uint32_t idBlue {id & 255};

    const std::string idInFGColor {esc + "[38;2;" + std::to_string(idRed) + ';'
            + std::to_string(idGreen) + ';' + std::to_string(idBlue) + 'm'};
    const std::string resetFGColor {esc + "[39m"};
    const std::string placeholderChar {"\U0010EEEE"};

    for (int r {0}; r < rows; ++r) {
        out += idInFGColor + placeholderChar + rowColDiacritics.data()[r];
        for (int c {1}; c < cols; ++c) {
            out += placeholderChar;
        }

        out += '\n';
    }
    out += resetFGColor;
}

void displayImg(const fs::path& imgAbs, std::string& out, int rows, int cols) {
    constexpr std::uint32_t minID {1};
    constexpr std::uint32_t maxID {(1 << 24) - 1};

    static std::mt19937 rng {std::random_device{}()};
    const std::uint32_t id {std::uniform_int_distribution{minID, maxID}(rng)};

    if (rows == 0 || cols == 0) {
        winsize winInfo {};
        ioctl(0, TIOCGWINSZ, &winInfo);
        const int cellXPix {winInfo.ws_xpixel / winInfo.ws_col};
        const int cellYPix {winInfo.ws_ypixel / winInfo.ws_row};

        int imgXPix {};
        int imgYPix {};
        int imgChannels {};
        stbi_info(imgAbs.c_str(), &imgXPix, &imgYPix, &imgChannels);

        const int rowsDesired {(imgYPix / cellYPix) + 1};
        const int colsDesired {(imgXPix / cellXPix) + 1};
        rows = rowsDesired;
        cols = colsDesired;

        if (rowsDesired > winInfo.ws_row) {
            rows = winInfo.ws_row;
            cols = static_cast<int>(static_cast<double>(rows) 
                                        / rowsDesired * cols) + 1;
        }
        if (colsDesired > winInfo.ws_col) {
            cols = winInfo.ws_col;
            rows = static_cast<int>(static_cast<double>(cols) 
                                        / colsDesired * rows) + 1;
        }
    }
    loadImg(imgAbs, id, rows, cols);
    displayLoadedImg(id, rows, cols, out);
    // fixs images breaking if multiple are displayed too fast in succession
    std::this_thread::sleep_for(std::chrono::milliseconds{5});
}
