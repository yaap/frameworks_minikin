#include <string.h>
#include <sys/stat.h>
#include <utils/Log.h>

#include <cstdio>
#include <fstream>
#include <vector>

#include "minikin/Hyphenator.h"

using minikin::HyphenationType;
using minikin::Hyphenator;
using minikin::HyphenatorCXX;

Hyphenator* loadHybFile(const char* fn, int minPrefix, int minSuffix, const char* language) {
    struct stat statbuf;
    int status = stat(fn, &statbuf);
    if (status < 0) {
        fprintf(stderr, "error opening %s\n", fn);
        return nullptr;
    }
    size_t size = statbuf.st_size;
    FILE* f = fopen(fn, "rb");
    if (f == NULL) {
        fprintf(stderr, "error opening %s\n", fn);
        return nullptr;
    }
    uint8_t* buf = new uint8_t[size];
    size_t read_size = fread(buf, 1, size, f);
    fclose(f);
    if (read_size < size) {
        fprintf(stderr, "error reading %s\n", fn);
        delete[] buf;
        return nullptr;
    }
    return HyphenatorCXX::loadBinary(buf, size, minPrefix, minSuffix, language);
}

int main(int argc, char** argv) {
    Hyphenator* hyph = loadHybFile(argv[1], 2, 3, "fr");  // should also be configurable
    std::vector<HyphenationType> result;
    std::vector<uint16_t> word;
    if (argc < 2) {
        fprintf(stderr, "usage: hyphtool word\n");
        return 1;
    }
    std::ifstream infile(argv[2]);
    std::string asciiword;
    while (std::getline(infile, asciiword)) {
        word.clear();
        result.clear();
        size_t len = asciiword.size();
        for (size_t i = 0; i < len; i++) {
            uint32_t c = asciiword[i];
            if (c == '-') {
                c = 0x00AD;
            }
            // ASCII (or possibly ISO Latin 1), but kinda painful to do utf conversion :(
            word.push_back(c);
        }
        hyph->hyphenate(word, &result);
        for (size_t i = 0; i < len; i++) {
            if (result[i] != HyphenationType::DONT_BREAK) {
                printf("-");
            }
            printf("%c", word[i]);
        }
        printf("\n");
    }
    return 0;
}
