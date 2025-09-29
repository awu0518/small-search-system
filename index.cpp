// g++ -O3 -std=c++20 index.cpp -o index

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>

uint64_t pack(uint32_t termID, uint32_t docID);
uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);

int main() {
    std::ifstream collection("collection.tsv");
    if (!collection) { std::cerr << "Unable to open collection.tsv"; exit(1); }

    std::unordered_map<std::string, uint32_t> lexicon;
    uint32_t currTermID = 0;

    std::unordered_map<uint32_t, double> pageTable;

    std::vector<uint64_t> buffer;

    std::string line = ""; 
    for (int i = 0; i < 1; i++) {
        std::getline(collection, line);
        size_t tab = line.find('\t');

        uint32_t docId = static_cast<uint32_t>(std::stoul(line.substr(0, tab)));
        std::string passage = line.substr(tab+1);
    }

    return 0;
}

/*
Returns a 64 bit unsigned integer in the following representation:
-- 32 bits (termID) -- 32 bits (docID) --
*/
uint64_t pack(uint32_t termID, uint32_t docID) {
    return (uint64_t(termID) << 32) | docID;
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
