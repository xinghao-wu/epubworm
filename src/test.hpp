#pragma once

// Test global constants depend on binary being ran with CWD being `build/`.
namespace test {
// non-self-validating
auto unzip() -> void;

auto getOPFRel() -> void;

auto getMetadata() -> void;

auto getTitle() -> void;

auto getAuthor() -> void;

auto findAndReplaceAll() -> void;

auto wrapForTmuxPassthrough() -> void;

auto wrapLines() -> void;

auto imageEscCodes() -> void;

auto imageChannels() -> void;

// non-self-validating
// Also exercises `loadImg()`.
auto displayImg() -> void;

// Also exercises `getHrefFromID()`.
auto getSpine() -> void;

// Also exercises `collectNavPoints()`.
auto getTOC() -> void;

// TODO: Make this self-validating.
// non-self-validating
// Also exercises `parseContentElem()`.
auto parseChapter() -> void;

// non-self-validating
auto dumpEpub() -> void;

// non-self-validating
// Also exercises `disableRawMode()`, `enableRawMode()`,
// `registerSigwinchHandler`, `handleSigwinch()`.
auto readRawInput() -> void;

auto utf8ToWide() -> void;

auto wideToUTF8() -> void;

auto collapseConsecutiveNewlines() -> void;

auto findNth() -> void;

auto execute() -> void;

auto getChapterProgressIndicator() -> void;

// non-self-validating
// Also exercises `setUpDisplayChapter()`,
// `snapTopLineToBound()`, `snapBotLineToBound()`,
// `calcBotLineFromTopLine()`, `calcTopLineFromBotLine()`,
// `inTmuxSession()`.
auto displayChapter() -> void;

auto tocDataToString() -> void;

// non-self-validating
// Also exercises `setUpDisplayTOC()`.
auto displayTOC() -> void;

// non-self-validating
auto displayEpub() -> void;

auto styleEachLineIndividually() -> void;

auto headingColors() -> void;

auto contentAlignment() -> void;

auto initConf() -> void;

auto initLibrary() -> void;

auto readConfig() -> void;

auto getTruncatedSHA256Sum() -> void;

auto findEpubById() -> void;

auto getUnambiguousEpubIdPrefix() -> void;

auto addToLibrary() -> void;

auto queryEpubElem() -> void;

auto writeProgress() -> void;

auto deleteFromLibrary() -> void;

// non-self-validating
auto readEpubInLibrary() -> void;

auto getLastRead() -> void;

auto singleInstanceLock() -> void;
} // namespace test
