#include <iostream>
#include <fstream>
#include <cstdint>

const int CHUNK_SIZE = 128;

struct Chunk {
    uint32_t docIds[CHUNK_SIZE];
    int freq[CHUNK_SIZE];
};

uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);

int main() {
    std::ifstream preIndex("mergedPreIndex");
    if (!preIndex) { std::cerr << "Failed to open pre index file"; exit(1); }

    uint64_t packedNum; int freq;
    while (preIndex) {
        Chunk currChunk{};
        int i = 0;
        for (; i < CHUNK_SIZE && preIndex >> packedNum >> freq; i++) {
            uint32_t termID = unpackTermID(packedNum);
            uint32_t docID = unpackDocID(packedNum);

            currChunk.docIds[i] = docID;
            currChunk.freq[i] = freq;
        }

        if (i == 0) { break; }
    }
}

/*
Returns the first 32 bits of the packed number, which is the termID
*/
uint32_t unpackTermID(uint64_t pack) {
    return uint32_t(pack >> 32);
}

/*
Returns the last 32 bits of the packed number, which is the docID
*/
uint32_t unpackDocID(uint64_t pack) {
    return uint32_t(pack & 0xffffffffu);
}