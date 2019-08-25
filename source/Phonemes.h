#pragma once

struct PhonemeInfo {
    unsigned int nOffset;
    unsigned int nLength;
};

extern const PhonemeInfo    g_nPhonemeInfo[];
extern const unsigned short g_nPhonemeData[];
