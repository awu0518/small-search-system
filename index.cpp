#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <limits>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <queue>

const uint32_t CHUNK_SIZE = 128;

struct InvertedList {
    std::vector<uint32_t> numBytes;
    std::vector<uint32_t> lastDocIDs;
    uint32_t firstChunkCounter = 0;
    bool written = false;
};

struct Context {
    InvertedList currList;
    uint8_t firstChunkElems = 0;
    uint8_t lastChunkElems = 0;
    uint32_t chunkCounter = 0;
    uint32_t lastTermID = UINT32_MAX;

    std::vector<uint32_t> docIDs;
    std::vector<uint8_t> compressedDocIDs;
    std::vector<uint8_t> freqs;
};

void fillTermToWord(std::vector<std::string>& termToWord);
uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void encodeNum(std::vector<uint8_t>& compressedDocIds, uint32_t num);

void startNewTerm(Context& ctx, std::ofstream& index, std::ofstream& lexicon, const std::string& term) {
    ctx.currList = InvertedList{};
    ctx.currList.firstChunkCounter = ctx.chunkCounter;
    ctx.currList.written = true;
    ctx.firstChunkElems = ctx.lastChunkElems = 0;

    lexicon << term << " " << index.tellp() << " ";
}

void addToChunk(Context& ctx, uint32_t docID, uint8_t freq) {
    uint32_t diff = ctx.docIDs.empty() ? docID : (docID - ctx.docIDs.back());
    encodeNum(ctx.compressedDocIDs, diff);
    ctx.docIDs.push_back(docID);
    ctx.freqs.push_back(freq);
}

void flushCurrentChunk(Context& ctx, std::ofstream& index) {
    if (ctx.docIDs.empty()) return;

    ctx.currList.lastDocIDs.push_back(ctx.docIDs.back());
    ctx.currList.numBytes.push_back(ctx.compressedDocIDs.size());

    uint8_t elems = ctx.docIDs.size();
    if (ctx.firstChunkElems == 0) ctx.firstChunkElems = elems;
    ctx.lastChunkElems = elems; 

    index.write(reinterpret_cast<const char*>(ctx.compressedDocIDs.data()), static_cast<std::streamsize>(ctx.compressedDocIDs.size()));
    index.write(reinterpret_cast<const char*>(ctx.freqs.data()), static_cast<std::streamsize>(ctx.freqs.size()));

    ctx.docIDs.clear();
    ctx.compressedDocIDs.clear();
    ctx.freqs.clear();
    ctx.chunkCounter++;
}

void finalizeTerm(Context& ctx, std::ofstream& lexicon, std::ofstream& index) {
    if (!ctx.currList.written) return;

    if (!ctx.docIDs.empty()) flushCurrentChunk(ctx, index);

    uint32_t numChunks = ctx.currList.numBytes.size();
    lexicon << int(ctx.firstChunkElems) << " " << int(numChunks) << " ";
    for (size_t i = 0; i < ctx.currList.numBytes.size(); i++) {
        lexicon << int(ctx.currList.numBytes[i]) << " " << int(ctx.currList.lastDocIDs[i]) << " ";
    }
    lexicon << int(ctx.lastChunkElems) << "\n";

    ctx.currList = InvertedList{};
    ctx.firstChunkElems = ctx.lastChunkElems = 0;
}

int main() {
    std::ifstream preIndex("mergedPreIndex");
    std::ofstream lexicon("lexicon");
    std::ofstream index("index", std::ios::binary);

    if (!preIndex) { std::cerr << "Failed to open preIndex\n"; exit(1); }
    if (!lexicon) { std::cerr << "Failed to open lexicon\n"; exit(1); }
    if (!index) { std::cerr << "Failed to open index\n"; exit(1); }

    std::vector<std::string> termToWord;
    fillTermToWord(termToWord);
    std::cout << "Filled TermToWord  " << termToWord.size() << std::endl;

    uint64_t packedNum; unsigned freq;
    Context ctx;

    while (preIndex >> packedNum >> freq) {
        uint32_t termID = unpackTermID(packedNum), docID = unpackDocID(packedNum);
        if (termID != ctx.lastTermID) {
            std::cout << "Writing term " << termID << " " << termToWord[termID] << " to lexicon\n";
            finalizeTerm(ctx, lexicon, index);
            startNewTerm(ctx, index, lexicon, termToWord[termID]);
            ctx.lastTermID = termID;
        }

        addToChunk(ctx, docID, freq);

        if (ctx.docIDs.size() == CHUNK_SIZE) {
            flushCurrentChunk(ctx, index);
        }
    }

    finalizeTerm(ctx, lexicon, index);

    preIndex.close(); lexicon.close(); index.close();
}

void fillTermToWord(std::vector<std::string>& termToWord) { 
    std::ifstream file("tempFiles/termToWord");
    std::string temp; 

    while (file >> temp) { termToWord.push_back(temp); }
    file.close();
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

/*
Appends a number encoded in varByte into compressedDocIds
*/
void encodeNum(std::vector<uint8_t>& compressedDocIds, uint32_t num) {
    while (num >= 128) {
        uint8_t currByte = 128 + (num & 127);
        compressedDocIds.push_back(currByte);
        num = num >> 7;
    }

    uint8_t last = static_cast<uint8_t>(num);
    compressedDocIds.push_back(last);
}