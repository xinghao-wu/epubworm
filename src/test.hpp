#pragma once

namespace test {
    void unzip();

    void getOPFRel();

    void getMetadata();

    void getTitle();

    void getAuthor();

    void findAndReplaceAll();

    void wrapForTmuxPassthrough();

    // also serves to test `loadImg()`, `displayLoadedImg()`,
    //  `getGraphicsEscCode()`
    void displayImg();

    // also serves to test `getHrefFromID()`, `decodePercentEncoding()`
    void getSpine();

    // also serves to test `collectNavPoints()`
    void getTOC();

    // also serves to test `parseContentElem()`
    void parseChapter();

    // also serves to test `expandEllipsesAndTabs()`, `getVisualLen()`,
    //  `centerOnScreen()`, `centerJustify()`, `processContentText()`,
    //  `getOccurences()`, `getInvisEscSeqLen()`, `useSystemLocale()`,
    //  `wrapLines()`
    void dumpEpub();

    // also serves to test `disableRawMode()`, `enableRawMode()`
    //  `registerSigwinchHandler`
    void readRawInput();

    void utf8ToWide();

    void wideToUTF8();

    void findNth();

    // also serves to test `execute()`, `handleSigwinch()`,
    //  `registerSigwinchHandler()`, `setUpDisplayChapter()`,
    //  `snapTopLineToBound()`, `snapBotLineToBound()`,
    //  `calcBotLineFromTopLine()`, `calcTopLineFromBotLine()`,
    //  `inTmuxSession()`
    void displayChapter();

    void tocDataToString();

    // also serves to test `setUpDisplayTOC()`
    void displayTOC();
}
