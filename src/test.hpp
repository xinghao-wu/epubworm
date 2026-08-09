#pragma once

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

// Also exercises `getHrefFromID()`, `decodePercentEncoding()`.
void getSpine();

// Also exercises `collectNavPoints()`.
void getTOC();

// non-self-validating
// Also exercises `parseContentElem()`.
void parseChapter();

// non-self-validating
// Also exercises `expandEllipsesAndTabs()`, `getVisualLen()`,
// `centerOnScreen()`, `centerJustify()`, `processContentText()`,
// `getOccurrences()`, `getInvisEscSeqLen()`, `useSystemLocale()`,
// `wrapLines()`.
void dumpEpub();

// non-self-validating
// Also exercises `disableRawMode()`, `enableRawMode()`,
// `registerSigwinchHandler`.
void readRawInput();

void utf8ToWide();

void wideToUTF8();

void findNth();

void execute();

// non-self-validating
// Also exercises `handleSigwinch()`,
// `registerSigwinchHandler()`, `setUpDisplayChapter()`,
// `snapTopLineToBound()`, `snapBotLineToBound()`,
// `calcBotLineFromTopLine()`, `calcTopLineFromBotLine()`,
// `inTmuxSession()`.
void displayChapter();

// non-self-validating
void tocDataToString();

// non-self-validating
// Also exercises `setUpDisplayTOC()`.
void displayTOC();

// non-self-validating
void displayEpub();

void styleEachLineIndividually();

// non-self-validating
void initConf();

// non-self-validating
void initLibrary();

// non-self-validating
void readMncConf();

void getTruncatedSHA256Sum();

void findEpubById();

// non-self-validating
// Also exercises `setLastRead()`.
void addToLibrary();

void queryEpubElem();

// Also exercises `queryEpubElem()`.
void writeProgress();

// non-self-validating
void deleteFromLibrary();

// non-self-validating
void readEpubInLibrary();

void getLastRead();
} // namespace test
