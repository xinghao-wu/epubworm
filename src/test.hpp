#pragma once

namespace test {
void unzip();

void getOPFRel();

void getMetadata();

void getTitle();

void getAuthor();

void findAndReplaceAll();

void wrapForTmuxPassthrough();

// Also serves to test `loadImg()`, `displayLoadedImg()`,
// `getGraphicsEscCode()`.
void displayImg();

// Also serves to test `getHrefFromID()`, `decodePercentEncoding()`.
void getSpine();

// Also serves to test `collectNavPoints()`.
void getTOC();

// Also serves to test `parseContentElem()`.
void parseChapter();

// Also serves to test `expandEllipsesAndTabs()`, `getVisualLen()`,
// `centerOnScreen()`, `centerJustify()`, `processContentText()`,
// `getOccurrences()`, `getInvisEscSeqLen()`, `useSystemLocale()`,
// `wrapLines()`.
void dumpEpub();

// Also serves to test `disableRawMode()`, `enableRawMode()`,
// `registerSigwinchHandler`.
void readRawInput();

void utf8ToWide();

void wideToUTF8();

void findNth();

void execute();

// Also serves to test `handleSigwinch()`,
// `registerSigwinchHandler()`, `setUpDisplayChapter()`,
// `snapTopLineToBound()`, `snapBotLineToBound()`,
// `calcBotLineFromTopLine()`, `calcTopLineFromBotLine()`,
// `inTmuxSession()`.
void displayChapter();

void tocDataToString();

// Also serves to test `setUpDisplayTOC()`.
void displayTOC();

void displayEpub();

void styleEachLineIndividually();

void initConf();

void initLibrary();

void readMncConf();

void getTruncatedSHA256Sum();

void addToLibrary();
} // namespace test
