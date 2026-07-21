#pragma once

namespace test {
    void unzip();

    void getOPFRel();

    void getMetadata();

    void getTitle();

    void getAuthor();

    void findAndReplaceAll();

    void wrapForTmuxPassthrough();

    // also serves to test loadImg(), displayLoadedImg(), getGraphicsEscCode()
    void displayImg();

    // also serves to test getHrefFromID(), decodePercentEncoding()
    void getSpine();

    // also serves to test collectNavPoints()
    void getTOC();

    // also serves to test parseContentElem()
    void parseChapter();

    // also serves to test expandEllipsesAndTabs(), getVisualLen(), 
    // centerOnScreenAndAddEraseLineSeq(), centerJustify(), getOccurences(),
    // getInvisEscSeqLen(), useSystemLocale(), wrapLines()
    void dumpEpub();

    // also serves to test disableRawMode(), enableRawMode()
    void rawReadKey();

    void utf8ToWide();

    void wideToUTF8();
}
