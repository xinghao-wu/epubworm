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

        constexpr int margin {1};
        if (rowsDesired > winInfo.ws_row - margin * 2) {
            rows = winInfo.ws_row - margin * 2;
            cols = static_cast<int>(static_cast<double>(rows) 
                                        / rowsDesired * cols) + 1;
        }
        if (colsDesired > winInfo.ws_col - margin * 4) {
            cols = winInfo.ws_col - margin * 4;
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
        throw std::runtime_error{"failed to restore original term settings"};
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &ogTermFlags) == -1) {
        throw std::runtime_error{"failed to get original terminal settings"};
    }

    std::atexit(disableRawMode);

    termios rawTermFlags {ogTermFlags};
    // disable echo and canonical mode
    rawTermFlags.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON));
    // let read() return 0 every 100ms when not receiving input
    rawTermFlags.c_cc[VMIN] = 0;
    rawTermFlags.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermFlags) == -1) {
        throw std::runtime_error{"failed to set term settings to raw mode"};
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
            chLen = 0;
        }
        totalLen += chLen;
    }
    totalLen -= getInvisEscSeqLen(str);
    return totalLen; 
}

void useSystemLocale() {
    std::locale::global(std::locale("")); 
    std::cout.imbue(std::locale{});
    std::cin.imbue(std::locale{});
}

void wrapLines(std::string& str, int maxLen) {
    for (std::size_t lineBeginIndex {0}, lineEndIndex {str.find('\n')};
            lineBeginIndex < str.size();
            lineBeginIndex = lineEndIndex + 1, 
            lineEndIndex = str.find('\n', lineBeginIndex)) {

        if (lineEndIndex == std::string::npos) {
            lineEndIndex = str.size() - 1;
        }

        const std::wstring wideLine {utf8ToWide(std::string_view{str}
                .substr(lineBeginIndex, lineEndIndex - lineBeginIndex + 1))};

        bool lineDone {false};
        std::size_t wideLineBreakIndex {};
        wideLineBreakIndex = wideLine.find_first_of(L" \n");
        while (wideLineBreakIndex != std::string::npos) {
            const std::wstring wideBeginToBreak {std::wstring_view{wideLine}
                    .substr(0, wideLineBreakIndex + 1)};

            int beginToBreakLen {getVisualLen(wideBeginToBreak)};
            if (beginToBreakLen > maxLen + 1) {
                break;
            }
            if (wideBeginToBreak.back() == L'\n') {
                if (beginToBreakLen <= maxLen) {
                    lineDone = true;
                    break;
                }
                if (beginToBreakLen == maxLen + 1) {
                    break;
                }
            }
            wideLineBreakIndex = 
                    wideLine.find_first_of(L" \n", wideLineBreakIndex + 1);
        }
        wideLineBreakIndex = wideLine.rfind(L' ', wideLineBreakIndex - 1);

        if (wideLineBreakIndex == std::string::npos || lineDone == true) {
            continue;
        }

        const std::string beginToLineBreak {wideToUTF8(
                std::wstring_view{wideLine}.substr(0, wideLineBreakIndex + 1))};
        const std::size_t lineBreakIndex 
                {lineBeginIndex + beginToLineBreak.size() - 1};

        str.replace(lineBreakIndex, 1, "\n");
        lineEndIndex = lineBreakIndex;
    }
}

void centerContentOnScreen(std::string& str, int maxLen) {
    winsize winInfo {};
    ioctl(0, TIOCGWINSZ, &winInfo);

    for (std::size_t lineBeginIndex {0}; lineBeginIndex < str.size();
            lineBeginIndex = str.find('\n', lineBeginIndex) + 1) {

        int contentWidth {};
        // check if line is part of image
        if (std::string_view{str}.substr(lineBeginIndex, 7) == esc + "[38;2;") {
            constexpr std::string_view imgCellCh {"\U0010EEEE"};

            const std::size_t lineEndIndex {str.find('\n', lineBeginIndex)};
            std::string_view line {std::string_view{str}.substr(
                    lineBeginIndex, lineEndIndex - lineBeginIndex + 1)};

            const int imgCols {getOccurences(line, imgCellCh)};

            contentWidth = imgCols;
        }
        else {
            contentWidth = maxLen;
        }

        int paddingLen = (winInfo.ws_col - contentWidth) / 2;
        if (paddingLen <= 0) {
            continue;
        }
        str.insert(lineBeginIndex, static_cast<std::size_t>(paddingLen), ' ');
    }
}

void centerJustify(std::string_view prefix, std::string_view postfix, 
                   std::string& str, int maxLen) {
    for (std::size_t specBeginIndex {str.find(prefix)}, 
            specEndIndex {str.find(postfix, specBeginIndex + prefix.size())};
            specBeginIndex != std::string::npos;
            specBeginIndex = str.find(prefix, specEndIndex + postfix.size()),
            specEndIndex = str.find(postfix, specBeginIndex + prefix.size())) {

        for (std::size_t lineBeginIndex {specBeginIndex},
                lineEndIndex {str.find('\n', lineBeginIndex) > specEndIndex
                              ? specEndIndex : str.find('\n', lineBeginIndex)};
                lineBeginIndex <= specEndIndex;
                lineBeginIndex = lineEndIndex + 1,
                lineEndIndex = str.find('\n',lineBeginIndex) > specEndIndex
                               ? specEndIndex : str.find('\n', lineBeginIndex)) {

            const std::wstring wideLine {utf8ToWide(std::string_view{str}.substr(
                        lineBeginIndex, lineEndIndex - lineBeginIndex + 1))};

            int lineVisualLen {getVisualLen(wideLine)};

            const std::size_t paddingLen 
                    {static_cast<std::size_t>((maxLen - lineVisualLen) / 2)};

            str.insert(lineBeginIndex, paddingLen, ' ');
            lineEndIndex += paddingLen;
            specEndIndex += paddingLen;
        }
    }
}

void clearScreen() {
    std::cout << esc << clearAll << esc << posCursorTopLeft;
}

int rawReadKey() {
    ssize_t err {};
    char ch {};
    while ((err = read(STDIN_FILENO, &ch, 1)) != 1) {
        if (err == -1 && errno != EAGAIN) {
            throw std::system_error{errno, std::generic_category(),
                                    "raw mode read key errored"};
        }
    }
    if (ch != '\033') {
        return ch;
    }

    std::vector<char> seq (3);
    if (read(STDIN_FILENO, &seq[0], 1) == 0) {
        return '\033';
    }
    read(STDIN_FILENO, &seq[1], 1);

    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            read(STDIN_FILENO, &seq[2], 1);
            if (seq[2] == '~') {
                switch (seq[1]) {
                case '1': return key::home;
                case '4': return key::end;
                case '5': return key::pgUp;
                case '6': return key::pgDown;
                case '7': return key::home;
                case '8': return key::end;
                }
            }
        } 
        else {
            switch (seq[1]) {
            case 'A': return key::arrowUp;
            case 'B': return key::arrowDown;
            case 'C': return key::arrowRight;
            case 'D': return key::arrowLeft;
            case 'H': return key::home;
            case 'F': return key::end;
            }
        }
    } 
    if (seq[0] == 'O') {
        switch (seq[1]) {
        case 'H': return key::home;
        case 'F': return key::end;
        }
    }
    return key::unknownEscSeq;
}
