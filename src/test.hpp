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
    // also serves to test getHrefFromID()
    void getSpine();
    // also serves to test collectNavPoints()
    void getTOC();
    void parseText();
}
