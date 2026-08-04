#include <algorithm>
#include <random>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <locale>
#include <thread>
#include <chrono>
#include <vector>
#include <tuple>
#include <utility>
#include <cstdlib>
#include <cwchar>
#include <cmath>
#include <cerrno>
#include <cstdint>
#include <csignal>
#include <cassert>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <iconv.h>
#include <asm-generic/ioctls.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <spawn.h>
#include <stdlib.h>
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
    if (inTmuxSession()) {
        wrapForTmuxPassthrough(graphicsEscCode);
    }
    std::cout << graphicsEscCode << std::flush;
}

void displayLoadedImg(std::uint32_t id, int rows, int cols, std::string& out) {
    if (id < 1 || id > static_cast<std::uint32_t>((1 << 24) - 1)) {
        throw std::runtime_error{"image id not in valid range"};
    }

    const std::uint32_t idRed {(id >> 16) & 255};
    const std::uint32_t idGreen {(id >> 8) & 255};
    const std::uint32_t idBlue {id & 255};
    const std::string idInFG {"[38;2;" + std::to_string(idRed) + ';'
            + std::to_string(idGreen) + ';' + std::to_string(idBlue) + 'm'};

    for (int r {0}; r < rows; ++r) {
        out += esc + idInFG;
        out += imgCellPlaceholder + rowColDiacritics.data()[r];

        for (int c {1}; c < cols; ++c) {
            out += imgCellPlaceholder;
        }
        out += esc + resetFG;
        out += '\n';
    }
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

        if (rowsDesired > winInfo.ws_row - 1) {
            rows = winInfo.ws_row - 1;
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
    std::cout << esc << disableDecimalReportingFormat;
    std::cout << esc << disableMouseEventReporting;
    std::cout << std::flush;

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

    std::cout << esc << enableDecimalReportingFormat;
    std::cout << esc << enableMouseEventReporting;
    std::cout << std::flush;

    registerSigwinchHandler();
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
    totalLen += getOccurences<std::wstring_view>(str, L"\033\\") * 1;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[2J") * 3;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[H") * 2;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[2K") * 3;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[?25l") * 5;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[?25h") * 5;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[?1003h") * 7;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[?1003l") * 7;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[?1006h") * 7;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[?1006l") * 7;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[1m") * 3;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[22m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[3m") * 3;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[23m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[33m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[31m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[32m") * 4;
    totalLen += getOccurences<std::wstring_view>(str, L"\033[34m") * 4;
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

            const int beginToBreakLen {getVisualLen(wideBeginToBreak)};
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
        const std::string_view line {std::string_view{str}.substr(
                lineBeginIndex, lineEndIndex - lineBeginIndex + 1)};

        if (line.contains(imgCellPlaceholder)) {
            const int imgCols {
                    getOccurences<std::string_view>(line, imgCellPlaceholder)};
            contentWidth = imgCols;
        }
        else {
            contentWidth = maxLen;
        }

        const int paddingLen = (winInfo.ws_col - contentWidth) / 2;
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

            const int lineVisualLen {getVisualLen(wideLine)};

            const std::size_t paddingLen {
                    static_cast<std::size_t>((maxLen - lineVisualLen) / 2)};

            str.insert(lineBeginIndex, paddingLen, ' ');
            lineEndIndex += paddingLen;
            specEndIndex += paddingLen;
        }
    }
}

std::tuple<Key, int, int> readRawInput() {
    ssize_t err {};
    char startCh {};
    while ((err = read(STDIN_FILENO, &startCh, 1)) != 1 && g_winResize == 0) {
        if (err == -1 && errno != EAGAIN && errno != EINTR) {
            throw std::system_error{errno, std::generic_category(),
                                    "raw mode read key errored"};
        }
    }
    if (g_winResize == 1) {
        g_winResize = 0;
        return {specKey::winResize, 0, 0};
    }
    if (startCh != '\033') {
        return {startCh, 0, 0};
    }

    std::string seq {};
    seq.resize(100);
    std::size_t i {0};
    while (read(STDIN_FILENO, &seq[i], 1) == 1) {
        switch (seq[i]) {
        case '~': case 'A': case 'B': case 'C': case 'D':
        case 'H': case 'F': case 'M': case 'm':
            ++i;
            goto exit_loop;
        }
        ++i;
    }
exit_loop:
    seq.resize(i);

    if (seq.empty()) {
        return {'\033', 0, 0};
    }

    if (seq == "[5~") return {specKey::pgUp, 0, 0};
    if (seq == "[6~") return {specKey::pgDown, 0, 0};
    if (seq == "[A") return {specKey::arrowUp, 0, 0};
    if (seq == "[B") return {specKey::arrowDown, 0, 0};
    if (seq == "[C") return {specKey::arrowRight, 0, 0};
    if (seq == "[D") return {specKey::arrowLeft, 0, 0};
    if (seq == "[1~" || esc == "[7~" || esc == "[H" || esc == "OH") {
        return {specKey::home, 0, 0};
    }
    if (seq == "[4~" || esc == "[8~" || esc == "[F" || esc == "OF") {
        return {specKey::end, 0, 0};
    }

    if (seq.starts_with("[<")) {
        assert(seq.find_first_of("mM") == seq.size() - 1
               && "received incomplete or multiple mouse actions");

        const std::size_t firstSemicolonIndex {findNth(seq, ";", 1)};
        const std::size_t secSemicolonIndex {findNth(seq, ";", 2)};
        const int action {std::stoi(seq.substr(2, firstSemicolonIndex - 2))};
        const int col {std::stoi(seq.substr(firstSemicolonIndex + 1,
                       secSemicolonIndex - firstSemicolonIndex - 1))};
        const int row {std::stoi(seq.substr(secSemicolonIndex + 1,
                       seq.find_first_of("mM") - secSemicolonIndex - 1))};

        if (seq.back() == 'm') {
            if (action == 0) return {specKey::leftClickRelease, row, col};
            if (action == 2) return {specKey::rightClickRelease, row, col};
        }
        if (seq.back() == 'M') {
            if (action == 64) return {specKey::wheelUp, row, col};
            if (action == 65) return {specKey::wheelDown, row, col};
        }
        return {specKey::unknown, row, col};
    }
    return {specKey::unknown, 0, 0};
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

void styleEachLineIndividually(std::string& str, std::string_view style,
                               std::string_view resetStyle) {
    std::size_t styleBeginIndex {str.find(style)};
    std::size_t styleEndIndex {
            str.find(resetStyle, styleBeginIndex + style.size())
            + resetStyle.size() - 1};
    while (styleBeginIndex != std::string::npos) {
        std::size_t styleNewlineIndex {str.find('\n', styleBeginIndex)};
        while (styleNewlineIndex < styleEndIndex) {
            str.insert(styleNewlineIndex, resetStyle);
            styleNewlineIndex += resetStyle.size();
            styleEndIndex += resetStyle.size();
            str.insert(styleNewlineIndex + 1, style);
            styleEndIndex += style.size();

            styleNewlineIndex = str.find('\n', styleNewlineIndex + 1);
        }

        styleBeginIndex = str.find(style, styleEndIndex + 1);
        styleEndIndex = str.find(resetStyle, styleBeginIndex + style.size())
                        + resetStyle.size() - 1;
    }
}

void processContentText(std::string& str, int maxLen) {
    expandEllipsesAndTabs(str);
    wrapLines(str, maxLen);
    centerJustify(esc + yellowFG, esc + resetFG, str, maxLen);
    centerJustify(esc + blueFG, esc + resetFG, str, maxLen);
    centerJustify(esc + redFG, esc + resetFG, str, maxLen);
    centerOnScreen(str, maxLen);
    styleEachLineIndividually(str, esc + bold, esc + resetBold);
    styleEachLineIndividually(str, esc + italic, esc + resetItalic);
    styleEachLineIndividually(str, esc + yellowFG, esc + resetFG);
    styleEachLineIndividually(str, esc + redFG, esc + resetFG);
    styleEachLineIndividually(str, esc + greenFG, esc + resetFG);
    styleEachLineIndividually(str, esc + blueFG, esc + resetFG);
}

std::pair<ChapterExit, double> displayChapter(
        const fs::path& chapterAbs, double iniProg, int desiredMaxLen) {
    winsize winInfo {};
    std::string chapter {};
    int chapterLines {};
    int screenTopLine {};
    int screenBotLine {};
    setUpDisplayChapter(chapterAbs, iniProg, desiredMaxLen, winInfo, chapter,
                        chapterLines, screenTopLine, screenBotLine);

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
        if (inTmuxSession()) {
            execute(std::vector<std::string>{"tmux", "refresh-client"});
        }

        const double prog {static_cast<double>(screenTopLine) / chapterLines};

        while (true) {
            std::tuple<Key, int, int> input {readRawInput()};
            Key translatedInputKey {std::get<0>(input)};
            if (translatedInputKey == specKey::leftClickRelease) {
                if (std::get<2>(input) <= winInfo.ws_col / 2) {
                    translatedInputKey = 'h';
                }
                else {
                    translatedInputKey = 'l';
                }
            }

            switch (translatedInputKey) {
            case 't': case '\t':
                return {ChapterExit::toc, prog};
            case 'q':
                return {ChapterExit::quit, prog};
            case 'h': case 'b': case ctrlB:
            case specKey::arrowLeft: case specKey::pgUp:
                if (screenTopLine == 1) {
                    return {ChapterExit::prev, prog};
                }
                screenTopLine -= winInfo.ws_row;
                snapTopLineToBound(screenTopLine);
                screenBotLine = calcBotLineFromTopLine(screenTopLine, winInfo);
                snapBotLineToBound(screenBotLine, chapterLines);
                goto redraw_screen;
            case 'l': case 'f': case ctrlF: case ' ':
            case specKey::arrowRight: case specKey::pgDown:
                if (screenBotLine == chapterLines) {
                    return {ChapterExit::next, prog};
                }
                screenBotLine += winInfo.ws_row;
                snapBotLineToBound(screenBotLine, chapterLines);
                screenTopLine = calcTopLineFromBotLine(screenBotLine, winInfo);
                snapTopLineToBound(screenTopLine);
                goto redraw_screen;
            case 'u': case ctrlU:
                if (screenTopLine == 1) {
                    return {ChapterExit::prev, prog};
                }
                screenTopLine -= winInfo.ws_row / 2;
                snapTopLineToBound(screenTopLine);
                screenBotLine = calcBotLineFromTopLine(screenTopLine, winInfo);
                snapBotLineToBound(screenBotLine, chapterLines);
                goto redraw_screen;
            case 'd': case ctrlD:
                if (screenBotLine == chapterLines) {
                    return {ChapterExit::next, prog};
                }
                screenBotLine += winInfo.ws_row / 2;
                snapBotLineToBound(screenBotLine, chapterLines);
                screenTopLine = calcTopLineFromBotLine(screenBotLine, winInfo);
                snapTopLineToBound(screenTopLine);
                goto redraw_screen;
            case 'k': case specKey::arrowUp: case specKey::wheelUp:
                if (screenTopLine == 1) {
                    return {ChapterExit::prev, prog};
                }
                --screenTopLine;
                --screenBotLine;
                goto redraw_screen;
            case 'j': case specKey::arrowDown: case specKey::wheelDown:
                if (screenBotLine == chapterLines) {
                    return {ChapterExit::next, prog};
                }
                ++screenTopLine;
                ++screenBotLine;
                goto redraw_screen;
            case 'g': case specKey::home:
                if (screenTopLine != 1) {
                    screenTopLine = 1;
                    screenBotLine =
                            calcBotLineFromTopLine(screenTopLine, winInfo);
                    snapBotLineToBound(screenBotLine, chapterLines);
                    goto redraw_screen;
                }
                break;
            case 'G': case specKey::end:
                if (screenBotLine != chapterLines) {
                    screenBotLine = chapterLines;
                    screenTopLine =
                            calcTopLineFromBotLine(screenBotLine, winInfo);
                    snapTopLineToBound(screenTopLine);
                    goto redraw_screen;
                }
                break;
            case specKey::winResize:
                setUpDisplayChapter(chapterAbs, prog, desiredMaxLen, winInfo,
                        chapter, chapterLines, screenTopLine, screenBotLine);
                goto redraw_screen;
            }
        }
redraw_screen:
    }
}

void setUpDisplayChapter(const fs::path& chapterAbs, double prog,
        int desiredMaxLen, winsize& winInfo, std::string& chapter,
        int& chapterLines, int& screenTopLine, int& screenBotLine) {

    ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

    chapter.clear();
    parseChapter(chapterAbs, chapter);
    const int maxLen {
            std::min(desiredMaxLen, static_cast<int>(winInfo.ws_col))};
    processContentText(chapter, maxLen);

    chapterLines = getOccurences<std::string_view>(chapter, "\n");

    screenTopLine = static_cast<int>(std::lround(prog * chapterLines));
    snapTopLineToBound(screenTopLine);

    screenBotLine = calcBotLineFromTopLine(screenTopLine, winInfo);
    snapBotLineToBound(screenBotLine, chapterLines);
    screenTopLine = calcTopLineFromBotLine(screenBotLine, winInfo);
    snapTopLineToBound(screenTopLine);
}

void snapTopLineToBound(int& screenTopLine) {
    screenTopLine = std::max(screenTopLine, 1);
}

void snapBotLineToBound(int& screenBotLine, int chapterLines) {
    screenBotLine = std::min(screenBotLine, chapterLines);
}

int calcBotLineFromTopLine(int screenTopLine, const winsize& winInfo) {
    return screenTopLine + winInfo.ws_row - 1;
}

int calcTopLineFromBotLine(int screenBotLine, const winsize& winInfo) {
    return screenBotLine - winInfo.ws_row + 1;
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
    const int spawnStatus {posix_spawnp(&pid, posixAPIArgV.front(), nullptr,
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

void tocDataToString(const TocData& data, std::string& str) {
    str += esc + blueFG;
    str += esc + bold;
    str += "Table of Contents";
    str += esc + resetFG;
    str += esc + resetBold;
    str += "\n\n";

    for (const auto& navPoint : data) {
        str += navPoint.first + "\n\n";
    }
    str.pop_back();

    str += esc + redFG;
    str += esc + bold;
    str += "---";
    str += esc + resetFG;
    str += esc + resetBold;
    str += '\n';
}

bool inTmuxSession() {
    const char* termProgram {std::getenv("TERM_PROGRAM")};
    return termProgram != nullptr && std::string_view{termProgram} == "tmux";
}

fs::path displayTOC(const TocData& tocData,
                    int desiredMaxLen, int selectedNavPointIndex) {
    winsize winInfo {};
    std::string tocStr {};
    int tocLines {};
    setUpDisplayTOC(tocData, desiredMaxLen, winInfo, tocStr, tocLines);

    while (true) {
        if (!(selectedNavPointIndex < std::ssize(tocData))) {
            throw std::logic_error{"selected nav point out of bounds"};
        }

        const std::size_t selectionBeginIndex {
                findNth(tocStr, "\n\n", selectedNavPointIndex + 1) + 2};
        std::size_t selectionEndIndex {};
        if (selectedNavPointIndex == std::ssize(tocData) - 1) {
            selectionEndIndex = tocStr.rfind('\n', tocStr.rfind('\n') - 1) - 1;
        }
        else {
            selectionEndIndex =
                    findNth(tocStr, "\n\n", selectedNavPointIndex + 2) - 1;
        }
        const int selectionBeginLine {
                getOccurences<std::string_view>(std::string_view{tocStr}
                .substr(0, selectionBeginIndex + 1), "\n") + 1};
        const int selectionEndLine {
                getOccurences<std::string_view>(std::string_view{tocStr}
                .substr(0, selectionEndIndex + 1), "\n") + 1};
        const int selectionLines {selectionEndLine - selectionBeginLine + 1};
        const int nonSelectionLines {winInfo.ws_row - selectionLines};

        int screenTopLine {selectionBeginLine - nonSelectionLines / 2};
        snapTopLineToBound(screenTopLine);
        int screenBotLine {calcBotLineFromTopLine(screenTopLine, winInfo)};
        snapBotLineToBound(screenBotLine, tocLines);
        screenTopLine = calcTopLineFromBotLine(screenBotLine, winInfo);
        snapTopLineToBound(screenTopLine);

        std::size_t dispBeginIndex {};
        if (screenTopLine == 1) {
            dispBeginIndex = 0;
        }
        else {
            dispBeginIndex = findNth(tocStr, "\n", screenTopLine - 1) + 1;
        }
        const std::size_t dispEndIndex {
                findNth(tocStr, "\n", screenBotLine) - 1};

        const std::string_view dispBeforeSelection {std::string_view{tocStr}
                .substr(dispBeginIndex, selectionBeginIndex - dispBeginIndex)};
        const std::string_view dispSelection {std::string_view{tocStr}
                .substr(selectionBeginIndex,
                        selectionEndIndex - selectionBeginIndex + 1)};
        const std::string_view dispAfterSelection {std::string_view{tocStr}
                .substr(selectionEndIndex + 1,
                        dispEndIndex - selectionEndIndex)};

        eraseScreen();
        std::cout << dispBeforeSelection;
        std::cout << esc << greenFG;
        std::cout << esc << bold;
        std::cout << dispSelection;
        std::cout << esc << resetFG;
        std::cout << esc << resetBold;
        std::cout << dispAfterSelection;
        std::cout << std::flush;

        while (true) {
            std::tuple<Key, int, int> input {readRawInput()};
            switch (std::get<0>(input)) {
            case 't': case '\t': case 'q': case '\033':
                return {};
            case '\n':
                return tocData.data()[selectedNavPointIndex].second;
            case 'h': case 'b': case ctrlB:
            case specKey::arrowLeft: case specKey::pgUp:
                if (selectedNavPointIndex != 0) {
                    selectedNavPointIndex -= winInfo.ws_row / 2;
                    selectedNavPointIndex = std::max(selectedNavPointIndex, 0);
                    goto redraw_screen;
                }
                break;
            case 'l': case 'f': case ctrlF: case ' ':
            case specKey::arrowRight: case specKey::pgDown:
                if (selectedNavPointIndex != std::ssize(tocData) - 1) {
                    selectedNavPointIndex += winInfo.ws_row / 2;
                    selectedNavPointIndex = std::min(selectedNavPointIndex,
                            static_cast<int>(tocData.size()) - 1);
                    goto redraw_screen;
                }
                break;
            case 'u': case ctrlU:
                if (selectedNavPointIndex != 0) {
                    selectedNavPointIndex -= winInfo.ws_row / 4;
                    selectedNavPointIndex = std::max(selectedNavPointIndex, 0);
                    goto redraw_screen;
                }
                break;
            case 'd': case ctrlD:
                if (selectedNavPointIndex != std::ssize(tocData) - 1) {
                    selectedNavPointIndex += winInfo.ws_row / 4;
                    selectedNavPointIndex = std::min(selectedNavPointIndex,
                            static_cast<int>(tocData.size()) - 1);
                    goto redraw_screen;
                }
                break;
            case 'k': case specKey::arrowUp:
                if (selectedNavPointIndex != 0) {
                    --selectedNavPointIndex;
                    goto redraw_screen;
                }
                break;
            case 'j': case specKey::arrowDown:
                if (selectedNavPointIndex != std::ssize(tocData) - 1) {
                    ++selectedNavPointIndex;
                    goto redraw_screen;
                }
                break;
            case 'g': case specKey::home:
                if (selectedNavPointIndex != 0) {
                    selectedNavPointIndex = 0;
                    goto redraw_screen;
                }
                break;
            case 'G': case specKey::end:
                if (selectedNavPointIndex != std::ssize(tocData) - 1) {
                    selectedNavPointIndex =
                            static_cast<int>(tocData.size() - 1);
                    goto redraw_screen;
                }
                break;
            case specKey::winResize:
                setUpDisplayTOC(tocData, desiredMaxLen,
                                winInfo, tocStr, tocLines);
                goto redraw_screen;
            }
        }
redraw_screen:
    }
}

void setUpDisplayTOC(const TocData& tocData, int desiredMaxLen,
                     winsize& winInfo, std::string& tocStr, int& tocLines) {
    ioctl(STDIN_FILENO, TIOCGWINSZ, &winInfo);

    tocStr.clear();
    tocDataToString(tocData, tocStr);
    const int maxLen {
            std::min(desiredMaxLen, static_cast<int>(winInfo.ws_col))};
    processContentText(tocStr, maxLen);

    tocLines = getOccurences<std::string_view>(tocStr, "\n");
}

EpubProg displayEpub(const EpubProg& iniProg, const fs::path& epubRootAbs,
                     int desiredMaxLen) {
    const fs::path opfAbs {epubRootAbs / getOPFRel(epubRootAbs)};
    XMLDocument opf {};
    opf.LoadFile(opfAbs.c_str());
    if (opf.Error()) {
        throw std::runtime_error{opf.ErrorStr()};
    }

    std::vector spineWithAbs {getSpine(opf)};
    for (auto& rel : spineWithAbs) {
        rel = opfAbs.parent_path() / rel;
    }

    TocData tocDataWithAbs {getTOC(spineWithAbs[0])};
    for (auto& pair : tocDataWithAbs) {
        pair.second = spineWithAbs[0].parent_path() / pair.second;
    }

    std::size_t spineIndex {1};
    while (spineWithAbs[spineIndex] != iniProg.chapterAbs) {
        ++spineIndex;
    }
    double chapterProg {iniProg.chapterProg};

    std::cout << esc << hideCursor;
    std::cout << esc << clearScreen;
    while (true) {
        const std::pair chapterOut {
                displayChapter(spineWithAbs[spineIndex],
                               chapterProg, desiredMaxLen)};
        switch (chapterOut.first) {
        case ChapterExit::prev:
            if (spineIndex != 1) {
                --spineIndex;
                chapterProg = 1;
            }
            else {
                chapterProg = 0;
            }
            break;
        case ChapterExit::next:
            if (spineIndex != spineWithAbs.size() - 1) {
                ++spineIndex;
                chapterProg = 0;
            }
            else {
                chapterProg = 1;
            }
            break;
        case ChapterExit::toc:
            {
                int iniNavPointIndex {0};
                for (int i {static_cast<int>(spineIndex)}; i >= 1; --i) {
                    for (int j {0}; j < std::ssize(tocDataWithAbs); ++j) {
                        if (spineWithAbs.data()[i]
                                == tocDataWithAbs.data()[j].second) {
                            iniNavPointIndex = j;
                            goto exit_nested_loops;
                        }
                    }
                }
exit_nested_loops:
                const fs::path tocOut {displayTOC(tocDataWithAbs,
                                       desiredMaxLen, iniNavPointIndex)};
                bool found {false};
                for (int i {1}; i < std::ssize(spineWithAbs); ++i) {
                    if (spineWithAbs.data()[i] == tocOut) {
                        found = true;
                        spineIndex = static_cast<std::size_t>(i);
                        chapterProg = 0;
                        break;
                    }
                }
                if (!found) {
                    chapterProg = chapterOut.second;
                }
                break;
            }
        case ChapterExit::quit:
            eraseScreen();
            std::cout << esc << showCursor;
            return {spineWithAbs[spineIndex], chapterOut.second};
        }
    }
}
