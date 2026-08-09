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
// Also serves to test `loadImg()`, `displayLoadedImg()`,
// `getGraphicsEscCode()`.
void displayImg();

// Also serves to test `getHrefFromID()`, `decodePercentEncoding()`.
void getSpine();

// Also serves to test `collectNavPoints()`.
void getTOC();

// non-self-validating
// Also serves to test `parseContentElem()`.
void parseChapter();

// non-self-validating
// Also serves to test `expandEllipsesAndTabs()`, `getVisualLen()`,
// `centerOnScreen()`, `centerJustify()`, `processContentText()`,
// `getOccurrences()`, `getInvisEscSeqLen()`, `useSystemLocale()`,
// `wrapLines()`.
void dumpEpub();

// non-self-validating
// Also serves to test `disableRawMode()`, `enableRawMode()`,
// `registerSigwinchHandler`.
void readRawInput();

void utf8ToWide();

void wideToUTF8();

void findNth();

void execute();

// non-self-validating
// Also serves to test `handleSigwinch()`,
// `registerSigwinchHandler()`, `setUpDisplayChapter()`,
// `snapTopLineToBound()`, `snapBotLineToBound()`,
// `calcBotLineFromTopLine()`, `calcTopLineFromBotLine()`,
// `inTmuxSession()`.
void displayChapter();

// non-self-validating
void tocDataToString();

// non-self-validating
// Also serves to test `setUpDisplayTOC()`.
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

// non-self-validating
// Also serves to test `findEpubById()`, `setLastRead()`.
void addToLibrary();

// Also serves to test `findEpubById()`.
void queryEpubElem();

// Also serves to test `queryEpubElem()`.
void writeProgress();

// non-self-validating
void deleteFromLibrary();

void getLastRead();
} // namespace test
