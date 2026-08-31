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

// non-self-validating
// Also exercises `loadImg()`, `displayLoadedImg()`,
// `getGraphicsEscCode()`.
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

// TODO: Make this self-validating.
// non-self-validating
void tocDataToString();

// non-self-validating
// Also exercises `setUpDisplayTOC()`.
void displayTOC();

// non-self-validating
void displayEpub();

void styleEachLineIndividually();

void initConf();

void initLibrary();

void readTeiConf();

void getTruncatedSHA256Sum();

void findEpubById();

// Also exercises `setLastRead()`.
void addToLibrary();

void queryEpubElem();

void writeProgress();

void deleteFromLibrary();

// non-self-validating
void readEpubInLibrary();

void getLastRead();

void singleInstanceLock();
} // namespace test
