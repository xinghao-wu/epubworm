#include <random>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <cwchar>
#include <cmath>
#include <cerrno>
#include <cstdint>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <iconv.h>
#include <asm-generic/ioctls.h>
#include <sys/ioctl.h>
#include <spawn.h>
#include <sys/wait.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.hpp"
#include "row_col_diacritics.hpp"
#include "epub_parser.hpp"
#include "tui.hpp"

namespace fs = std::filesystem;

static termios g_ogTermFlags {};
volatile std::sig_atomic_t g_winResize {0};

void loadImg(const fs::path& imgAbs, std::uint32_t id, int rows, int cols) {
    if (id == 0) {
        throw std::runtime_error{"image id not in valid range"};
    }

    int xPixels {};
    int yPixels {};
    int channels {};
    constexpr int noRequiredChannelNum {0};

    unsigned char* pixelData {stbi_load(imgAbs.c_str(), &xPixels, &yPixels,
                                        &channels, noRequiredChannelNum)};
    if (pixelData == nullptr) {
        throw std::runtime_error{stbi_failure_reason()};
    }

    const int pixelDataSize {xPixels * yPixels * channels};
    const std::string_view pixelDataView {
            reinterpret_cast<const char*>(pixelData),
            static_cast<std::size_t>(pixelDataSize)};

    const fs::path tempDataFileAbs {
            "/dev/shm/mnc-img-data-tty-graphics-protocol"};
    std::ofstream tempDataFile {tempDataFileAbs};
    if (!tempDataFile.is_open()) {
        throw std::runtime_error{"image temp data file failed to open"};
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
        throw std::runtime_error{"image id not in valid range"};
    }

    const std::uint32_t idRed {(id >> 16) & 255};
    const std::uint32_t idGreen {(id >> 8) & 255};
    const std::uint32_t idBlue {id & 255};
    const std::string idInFG {esc + "[38;2;" + std::to_string(idRed) + ';'
            + std::to_string(idGreen) + ';' + std::to_string(idBlue) + 'm'};

    out += idInFG;
    for (int r {0}; r < rows; ++r) {
        out += imgCellPlaceholder + rowColDiacritics.data()[r];

        for (int c {1}; c < cols; ++c) {
            out += imgCellPlaceholder;
        }
        out += '\n';
    }
    out += esc + resetFG;
}

void displayImg(const fs::path& imgAbs, std::string& out, int rows, int cols) {
    constexpr std::uint32_t minID {1};
    constexpr std::uint32_t maxID {(1 << 24) - 1};

    static std::mt19937 s_rng {std::random_device{}()};
    const std::uint32_t id {
            std::uniform_int_distribution{minID, maxID}(s_rng)};

    if (rows == 0 || cols == 0) {
        winsize winInfo {};
        ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);
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
        if (rowsDesired > winInfo.ws_row - (margin * 2)) {
            rows = winInfo.ws_row - (margin * 2);
            cols = static_cast<int>(static_cast<double>(rows) 
                                    / rowsDesired * cols) + 1;
        }
        if (colsDesired > winInfo.ws_col - (margin * 4)) {
            cols = winInfo.ws_col - (margin * 4);
            rows = static_cast<int>(static_cast<double>(cols) 
                                    / colsDesired * rows) + 1;
        }
    }
    loadImg(imgAbs, id, rows, cols);
    displayLoadedImg(id, rows, cols, out);
    // fixs images breaking if multiple are displayed too fast in succession
    std::this_thread::sleep_for(std::chrono::milliseconds{5});
}

std::string getGraphicsEscCode(const fs::path& tempDataFileAbs, int channels, 
                               int xPixels, int yPixels, std::uint32_t id, 
                               int rows, int cols) {
    std::string ctrlData {};
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

    const std::string tempDataFileAbsEncoded {
            base64::to_base64(tempDataFileAbs.string())};

    return esc + "_G" + ctrlData + ';' + tempDataFileAbsEncoded + escEnd;
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_ogTermFlags) == -1) {
        throw std::runtime_error{"failed to restore original term settings"};
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &g_ogTermFlags) == -1) {
        throw std::runtime_error{"failed to get original terminal settings"};
    }

    if (std::atexit(disableRawMode) != 0) {
        throw std::runtime_error{"failed to register disableRawMode() to run "
                                 "at program exit"};
    }

    termios rawTermFlags {g_ogTermFlags};
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

    iconv_t convDescriptor {iconv_open("WCHAR_T", "UTF-8")};
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    if (convDescriptor == reinterpret_cast<iconv_t>(-1)) {
        throw std::system_error{errno, std::generic_category(), 
                                "iconv_open failed"};
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
        throw std::system_error{err, std::generic_category(), 
                                "iconv conversion failed"};
    }

    iconv_close(convDescriptor);
    
    const std::size_t bytesWritten {(output.size() * sizeof(wchar_t)) 
                                    - outBytesLeft};
    output.resize(bytesWritten / sizeof(wchar_t));

    return output;
}

std::string wideToUTF8(std::wstring_view input) {
    if (input.empty()) {
        return {};
    }

    iconv_t convDescriptor {iconv_open("UTF-8", "WCHAR_T")};
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    if (convDescriptor == reinterpret_cast<iconv_t>(-1)) {
        throw std::system_error{errno, std::generic_category(), 
                                "iconv_open failed"};
    }

    char* inBuf {const_cast<char*>(
                 reinterpret_cast<const char*>(input.data()))};
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
        throw std::system_error{err, std::generic_category(), 
                                "iconv conversion failed"};
    }

    iconv_close(convDescriptor);
    
    const std::size_t bytesWritten {output.size() - outBytesLeft};
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

int getInvisEscSeqLen(std::wstring_view str) {
    int totalLen {0};
    totalLen += getOccurences<std::wstring_view>(str, L"\033[1m") * 3;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[22m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[3m") * 3;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[23m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[33m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[31m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[39m") * 4;
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

        if (lineEndIndex == lineBeginIndex) {
            continue;
        }
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

        if (wideLineBreakIndex == std::string::npos || lineDone) {
            continue;
        }

        const std::string beginToLineBreak {wideToUTF8(std::wstring_view{
                wideLine}.substr(0, wideLineBreakIndex + 1))};
        const std::size_t lineBreakIndex {
                lineBeginIndex + beginToLineBreak.size() - 1};

        str.replace(lineBreakIndex, 1, "\n");
        lineEndIndex = lineBreakIndex;
    }
}

void centerOnScreen(std::string& str, int maxLen) {
    winsize winInfo {};
    ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

    for (std::size_t lineBeginIndex {0}; lineBeginIndex < str.size();
            lineBeginIndex = 
                (str.find('\n', lineBeginIndex) == std::string::npos)
                ? std::string::npos : str.find('\n', lineBeginIndex) + 1) {

        const std::size_t lineEndIndex {str.find('\n', lineBeginIndex)};
        if (lineEndIndex == lineBeginIndex) {
            continue;
        }

        int contentWidth {};
        std::string_view line {std::string_view{str}.substr(
                lineBeginIndex, lineEndIndex - lineBeginIndex + 1)};

        if (line.contains(imgCellPlaceholder)) {
            const int imgCols {
                    getOccurences<std::string_view>(line, imgCellPlaceholder)};
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

            if (lineEndIndex == lineBeginIndex) {
                continue;
            }

            const std::wstring wideLine {
                    utf8ToWide(std::string_view{str}.substr(
                    lineBeginIndex, lineEndIndex - lineBeginIndex + 1))};

            int lineVisualLen {getVisualLen(wideLine)};

            const std::size_t paddingLen {
                    static_cast<std::size_t>((maxLen - lineVisualLen) / 2)};

            str.insert(lineBeginIndex, paddingLen, ' ');
            lineEndIndex += paddingLen;
            specEndIndex += paddingLen;
        }
    }
}

// TODO: add SIGWINCH (window resize) handler
int rawReadKey() {
    ssize_t err {};
    char ch {};
    while ((err = read(STDIN_FILENO, &ch, 1)) != 1) {
        if (err == -1 && errno != EAGAIN && errno != EINTR) {
            throw std::system_error{errno, std::generic_category(),
                                    "raw mode read key errored"};
        }
    }
    if (ch != '\033') {
        return ch;
    }

    std::vector<char> seq (3);
    // NOLINTNEXTLINE(readability-container-data-pointer)
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

void eraseScreen() {
    winsize winInfo {};
    ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

    std::cout << esc << posCursorTopLeft;
    for (int i {0}; i < winInfo.ws_row; ++i) {
        std::cout << esc << eraseLine;
        if (i < winInfo.ws_row - 1) {
            std::cout << '\n';
        }
    }
    std::cout << esc << posCursorTopLeft;
}

void processContentText(std::string& str, int maxLen) {
    expandEllipsesAndTabs(str);
    wrapLines(str, maxLen);
    centerJustify(esc + yellowFG, esc + resetFG, str, maxLen);
    centerJustify(esc + redFG, esc + resetFG, str, maxLen);
    centerOnScreen(str, maxLen);
}

std::pair<int, double> displayChapter(const fs::path& chapterAbs,
                                      double iniProg, int maxLen) {
    std::string chapter {};
    parseChapter(chapterAbs, chapter);
    processContentText(chapter, maxLen);
    
    winsize winInfo {};
    ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

    const int chapterLines {getOccurences<std::string_view>(chapter, "\n")};

    int screenTopLine {static_cast<int>(std::lround(iniProg * chapterLines))};
    screenTopLine = std::max(screenTopLine, 1);

    int screenBotLine {screenTopLine + winInfo.ws_row - 1};
    screenBotLine = std::min(screenBotLine, chapterLines);
    screenTopLine = screenBotLine - winInfo.ws_row + 1;
    screenTopLine = std::max(screenTopLine, 1);

    std::cout << esc << hideCursor;
    while (true) {
        std::size_t dispBeginIndex {};
        if (screenTopLine == 1) {
            dispBeginIndex = 0;
        }
        else {
            dispBeginIndex = findNth(chapter, "\n", screenTopLine - 1) + 1;
        }
        const std::size_t dispEndIndex {
                findNth(chapter, "\n", screenBotLine) - 1};
        const std::string_view dispView {std::string_view{chapter}
                .substr(dispBeginIndex, dispEndIndex - dispBeginIndex + 1)};

        eraseScreen();
        std::cout << dispView << std::flush;
        // in ghostty, images on right-side tmux panes are broken until redraw
        execute(std::vector<std::string>{"tmux", "refresh-client"});

        const double prog {static_cast<double>(screenTopLine) / chapterLines};

        int inputKey {rawReadKey()};
        switch (inputKey) {
        case 't': case '\t': case 'q':
            std::cout << esc << showCursor;
            return {inputKey, prog};
        case 'h': case 'b': case key::arrowLeft: case key::pgUp:
            if (screenTopLine == 1) {
                std::cout << esc << showCursor;
                return {inputKey, prog};
            }
            screenTopLine -= winInfo.ws_row;
            screenTopLine = std::max(screenTopLine, 1);
            screenBotLine = screenTopLine + winInfo.ws_row - 1;
            screenBotLine = std::min(screenBotLine, chapterLines);
            break;
        case 'l': case 'f': case ' ': case key::arrowRight: case key::pgDown:
            if (screenBotLine == chapterLines) {
                std::cout << esc << showCursor;
                return {inputKey, prog};
            }
            screenBotLine += winInfo.ws_row;
            screenBotLine = std::min(screenBotLine, chapterLines);
            screenTopLine = screenBotLine - winInfo.ws_row + 1;
            screenTopLine = std::max(screenTopLine, 1);
            break;
        case 'u':
            if (screenTopLine == 1) {
                std::cout << esc << showCursor;
                return {inputKey, prog};
            }
            screenTopLine -= winInfo.ws_row / 2;
            screenTopLine = std::max(screenTopLine, 1);
            screenBotLine = screenTopLine + winInfo.ws_row - 1;
            screenBotLine = std::min(screenBotLine, chapterLines);
            break;
        case 'd':
            if (screenBotLine == chapterLines) {
                std::cout << esc << showCursor;
                return {inputKey, prog};
            }
            screenBotLine += winInfo.ws_row / 2;
            screenBotLine = std::min(screenBotLine, chapterLines);
            screenTopLine = screenBotLine - winInfo.ws_row + 1;
            screenTopLine = std::max(screenTopLine, 1);
            break;
        case 'k': case key::arrowUp:
            if (screenTopLine == 1) {
                std::cout << esc << showCursor;
                return {inputKey, prog};
            }
            --screenTopLine;
            --screenBotLine;
            break;
        case 'j': case key::arrowDown:
            if (screenBotLine == chapterLines) {
                std::cout << esc << showCursor;
                return {inputKey, prog};
            }
            ++screenTopLine;
            ++screenBotLine;
            break;
        case 'g': case key::home:
            screenTopLine = 1;
            screenBotLine = screenTopLine + winInfo.ws_row - 1;
            screenBotLine = std::min(screenBotLine, chapterLines);
            break;
        case 'G': case key::end:
            screenBotLine = chapterLines;
            screenTopLine = screenBotLine - winInfo.ws_row + 1;
            screenTopLine = std::max(screenTopLine, 1);
            break;
        }
    }
}

void execute(const std::vector<std::string>& argV) {
    if (argV.empty() || argV.front().empty()) {
        throw std::invalid_argument{"execute() cmd cannot be empty"};
    }
    
    std::vector<char*> posixAPIArgV {};
    posixAPIArgV.reserve(argV.size() + 1);
    for (const auto& arg : argV) {
        posixAPIArgV.push_back(const_cast<char*>(arg.c_str()));
    }
    posixAPIArgV.push_back(nullptr);

    pid_t pid {};
    int spawnStatus {posix_spawnp(&pid, posixAPIArgV.front(), nullptr, 
                                  nullptr, posixAPIArgV.data(), environ)};
    if (spawnStatus != 0) {
        throw std::system_error{spawnStatus, std::generic_category(), 
                                "failed to spawn cmd: " + argV.front()};
    }

    int waitStatus {};
    while (waitpid(pid, &waitStatus, 0) == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            throw std::system_error{errno, std::generic_category(), 
                                    "error waiting for cmd: " + argV.front()};
        }
    }
    if (!WIFEXITED(waitStatus) || WEXITSTATUS(waitStatus) != 0) {
        throw std::runtime_error{"cmd did not exit properly: " + argV.front()};
    }
}

extern "C" void handleSigwinch([[maybe_unused]] int signal) {
    g_winResize = 1;
}

void registerSigwinchHandler() {
    struct sigaction sigAct {};
    sigAct.sa_handler = handleSigwinch;
    sigemptyset(&sigAct.sa_mask);       // block no other signals when handling
    sigAct.sa_flags = SA_RESTART;       // restart interrupted system calls

    if (sigaction(SIGWINCH, &sigAct, nullptr) == -1) {
        throw std::runtime_error{"failed to register SIGWINCH handler"};
    }
}
