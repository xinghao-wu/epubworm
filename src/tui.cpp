#include <random>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
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
    std::cout << graphicsEscCode;
}

void displayLoadedImg(std::uint32_t id, int rows, int cols) {
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
        std::string placeholders {placeholderChar + rowColDiacritics.data()[r]};
        for (int c {1}; c < cols; ++c) {
            placeholders += placeholderChar;
        }

        std::cout << idInFGColor << placeholders << '\n';
    }
    std::cout << resetFGColor;
}

void displayImg(const fs::path& imgAbs, int rows, int cols) {
    constexpr std::uint32_t minID {1};
    constexpr std::uint32_t maxID {(1 << 24) - 1};

    static std::mt19937 rng {std::random_device{}()};
    const std::uint32_t id {std::uniform_int_distribution{minID, maxID}(rng)};

    loadImg(imgAbs, id, rows, cols);
    displayLoadedImg(id, rows, cols);
}
