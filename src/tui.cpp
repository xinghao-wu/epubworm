#include <asm-generic/ioctls.h>
#include <random>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <cerrno>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <sys/ioctl.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <termios.h>
#include <unistd.h>
#include <iconv.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.hpp"
#include "row_col_diacritics.hpp"
#include "tui.hpp"

namespace fs = std::filesystem;

static termios ogTermFlags {};

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

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ogTermFlags) == -1) {
        throw std::runtime_error{"failed to restore original terminal settings"};
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &ogTermFlags) == -1) {
        throw std::runtime_error{"failed to get original terminal settings"};
    }

    std::atexit(disableRawMode);

    termios rawTermFlags {ogTermFlags};
    rawTermFlags.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON));

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermFlags) == -1) {
        throw std::runtime_error{"failed to set terminal settings to raw mode"};
    }
}

std::wstring utf8ToWide(std::string_view input) {
    if (input.empty()) {
        return {};
    }

    const iconv_t convDescriptor {iconv_open("WCHAR_T", "UTF-8")};
    if (convDescriptor == reinterpret_cast<iconv_t>(-1)) {
        throw std::system_error(errno, std::generic_category(), 
                                "iconv_open failed");
    }

    char* inBuf {const_cast<char*>(input.data())};
    std::size_t inBytesLeft {input.size()};

    std::wstring output {};
    output.resize(input.size()); 
    char* outBuf {reinterpret_cast<char*>(output.data())};
    std::size_t outBytesLeft {output.size() * sizeof(wchar_t)};

    const std::size_t error {iconv(convDescriptor, &inBuf, &inBytesLeft, 
                                   &outBuf, &outBytesLeft)};
    if (error == static_cast<std::size_t>(-1)) {
        const int err {errno};
        iconv_close(convDescriptor);
        throw std::system_error(err, std::generic_category(), 
                                "iconv conversion failed");
    }

    iconv_close(convDescriptor);
    
    std::size_t bytesWritten {(output.size() * sizeof(wchar_t)) - outBytesLeft};
    output.resize(bytesWritten / sizeof(wchar_t));

    return output;
}

std::string wideToUTF8(std::wstring_view input) {
    if (input.empty()) {
        return {};
    }

    const iconv_t convDescriptor {iconv_open("UTF-8", "WCHAR_T")};
    if (convDescriptor == reinterpret_cast<iconv_t>(-1)) {
        throw std::system_error(errno, std::generic_category(), 
                                "iconv_open failed");
    }

    char* inBuf {const_cast<char*>(reinterpret_cast<const char*>(input.data()))};
    std::size_t inBytesLeft {input.size() * sizeof(wchar_t)};

    std::string output {};
    output.resize(input.size() * sizeof(wchar_t)); 
    char* outBuf {output.data()};
    std::size_t outBytesLeft {output.size()};

    const std::size_t error {iconv(convDescriptor, &inBuf, &inBytesLeft, 
                                   &outBuf, &outBytesLeft)};
    if (error == static_cast<std::size_t>(-1)) {
        const int err {errno};
        iconv_close(convDescriptor);
        throw std::system_error(err, std::generic_category(), 
                                "iconv conversion failed");
    }

    iconv_close(convDescriptor);
    
    std::size_t bytesWritten {output.size() - outBytesLeft};
    output.resize(bytesWritten);

    return output;
}

int getVisualLen(std::wstring_view str) {
    int totalLen {0};
    for (const auto& ch : str) {
        int chLen {wcwidth(ch)};
        if (chLen == -1) {
            chLen = 1;
        }
        totalLen += chLen;
    }
    // Why's the total length one less than its supposed to be? Hell if I know!
    return totalLen + 1; 
}

// TODO: optimization possible via looking for `wideLineBreak` forwards
void wrapLines(std::string& str, int maxLen) {
    if (str.back() != '\n') {
        str += '\n';
    }
    for (std::size_t lineBegin {0}, lineEnd {str.find('\n')};
            lineEnd != std::string::npos;
            lineBegin = lineEnd + 1, lineEnd = str.find('\n', lineBegin)) {

        const std::wstring wideLine {utf8ToWide(std::string_view
                {str.begin() + static_cast<std::ptrdiff_t>(lineBegin),
                 str.begin() + static_cast<std::ptrdiff_t>(lineEnd)})};

        std::size_t wideLineBreak {wideLine.size() - 1};
        while (wideLineBreak != std::string::npos && 
                getVisualLen(std::wstring_view{wideLine.begin(), wideLine.begin()
                             + static_cast<std::ptrdiff_t>(wideLineBreak)}) 
                > (wideLine[wideLineBreak] == L' ' ? maxLen + 1 : maxLen)) {
            wideLineBreak = wideLine.rfind(L' ', wideLineBreak - 1);
        }

        if (wideLineBreak == std::string::npos 
                || wideLineBreak == wideLine.size() - 1) {
            continue;
        }

        const std::string lineBreakAndAfter 
                {wideToUTF8(std::wstring_view{wideLine}.substr(wideLineBreak))};
        const std::size_t lineBreak {str.rfind(lineBreakAndAfter, lineEnd)};

        str.replace(lineBreak, 1, "\n");
        lineEnd = lineBreak;
    }
}

void centerContentOnScreen(std::string& str, int maxTextLen) {
    winsize winInfo {};
    ioctl(0, TIOCGWINSZ, &winInfo);

    for (std::size_t lineBeginIndex {0}; lineBeginIndex < str.size();
            lineBeginIndex = str.find('\n', lineBeginIndex) + 1) {

        int contentWidth {};
        // check if line is part of image
        if (std::string_view{str}.substr(lineBeginIndex, 7) == esc + "[38;2;") {
            constexpr std::string_view imgCellCh {"\U0010EEEE"};
            const std::size_t lineEndIndex {str.find('\n', lineBeginIndex)};
            int imgWidth {0};
            for (std::size_t cellPos {str.find(imgCellCh, lineBeginIndex)};
                    cellPos < lineEndIndex;
                    cellPos = str.find(imgCellCh, cellPos + imgCellCh.size())) {
                ++imgWidth;
            }

            contentWidth = imgWidth;
        }
        else {
            contentWidth = maxTextLen;
        }

        int paddingLen = (winInfo.ws_col - contentWidth) / 2;
        if (paddingLen <= 0) {
            continue;
        }
        str.insert(lineBeginIndex, static_cast<std::size_t>(paddingLen), ' ');
    }
}
