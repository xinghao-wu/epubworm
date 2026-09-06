#pragma once

// Test global constants depend on binary being ran with CWD being `build/`.
namespace test {
// non-self-validating
void unzip();

void getOPFRel();

void getMetadata();

void getTitle();

void getAuthor();

void findAndReplaceAll();

void wrapForTmuxPassthrough();

void imageEscCodes();

// non-self-validating
// Also exercises `loadImg()`.
void displayImg();

// Also exercises `getHrefFromID()`.
void getSpine();

// Also exercises `collectNavPoints()`.
void getTOC();

// TODO: Make this self-validating.
// non-self-validating
// Also exercises `parseContentElem()`.
void parseChapter();

// non-self-validating
void dumpEpub();

// non-self-validating
// Also exercises `disableRawMode()`, `enableRawMode()`,
// `registerSigwinchHandler`, `handleSigwinch()`.
void readRawInput();

void utf8ToWide();

void wideToUTF8();

void collapseConsecutiveNewlines();

void findNth();

void execute();

// non-self-validating
// Also exercises `setUpDisplayChapter()`,
// `snapTopLineToBound()`, `snapBotLineToBound()`,
// `calcBotLineFromTopLine()`, `calcTopLineFromBotLine()`,
// `inTmuxSession()`.
void displayChapter();

void tocDataToString();

// non-self-validating
// Also exercises `setUpDisplayTOC()`.
void displayTOC();

// non-self-validating
void displayEpub();

void styleEachLineIndividually();

void headingColors();

void initConf();

void initLibrary();

void readConfig();

void getTruncatedSHA256Sum();

void findEpubById();

void getUnambiguousEpubIdPrefix();

void addToLibrary();

void queryEpubElem();

void writeProgress();

void deleteFromLibrary();

// non-self-validating
void readEpubInLibrary();

void getLastRead();

void singleInstanceLock();
} // namespace test
