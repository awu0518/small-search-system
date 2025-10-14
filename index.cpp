// assume 6400b blocks, lets say 10 chunks per block, chunks will contain 128 entries of docids/freq
// max 32bits per docid, 1byte per freq
// each entry in the mergedPreIndex looks like (packedNum, freq) = (termid + docid, freq)
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <regex>
#include <filesystem>
#include "Block.h"
using namespace std; // fuck u alex


struct lexiconData {
    uint32_t startByte;
    uint32_t startChunk;
    uint32_t numChunks;
    uint32_t elemsFirstChunk;
    uint32_t elemsLastChunk;
    uint32_t firstChunkPos;
    uint32_t lastChunkPos;
    std::vector<std::pair<uint32_t, int>> nextBytes_lastDocID;

};

void printLex(const std::unordered_map<uint32_t, lexiconData>& lexicon, uint32_t termID, size_t maxEntries = 20) {
    auto it = lexicon.find(termID);
    if (it == lexicon.end()) {
        std::cerr << "Term ID " << termID << " not found in lexicon.\n";
        return;
    }

    const lexiconData& lex = it->second;

    std::cout << "lexicon[" << termID << "] {\n"
              << "  startByte: " << lex.startByte << "\n"
              << "  startChunk: " << lex.startChunk << "\n"
              << "  numChunks: " << lex.numChunks << "\n"
              << "  elemsFirstChunk: " << lex.elemsFirstChunk << "\n"
              << "  elemsLastChunk: " << lex.elemsLastChunk << "\n"
              << "  firstChunkPos: " << lex.firstChunkPos << "\n"
              << "  lastChunkPos: " << lex.lastChunkPos << "\n"
              << "  nextBytes_lastDocID: [\n";

    size_t toPrint = std::min(maxEntries, lex.nextBytes_lastDocID.size());
    for (size_t i = 0; i < toPrint; i++) {
        const auto& p = lex.nextBytes_lastDocID[i];
        std::cout << "    { nextBytes: " << p.first
                  << ", lastDocID: " << p.second << " }\n";
    }

    if (toPrint < lex.nextBytes_lastDocID.size()) {
        std::cout << "    ... (" << lex.nextBytes_lastDocID.size() - toPrint
                  << " more entries)\n";
    }

    std::cout << "  ]\n}\n";

    std::cout << "  ]\n}\n";
}

uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void arrDifferences(uint32_t* arr, int start, int end);
uint16_t encodeNum(std::ofstream* output, uint32_t num);
// void printArr(uint32_t* arr, int size);
void readVector(std::vector<std::string>& words);
void byteWrite(std::ofstream* output, uint32_t num, int size);
void writeLex(std::ofstream& lexFile, std::unordered_map<uint32_t, lexiconData> lexicon,  vector<string>& termtoWord);
uint32_t invListSize(std::unordered_map<uint32_t, lexiconData>& lexicon, uint32_t termid);
void updateLastDocID(std::unordered_map<uint32_t, lexiconData>& lexicon, uint32_t termid, int lastDocID);
int main() {
    std::ifstream preind("mergedPreIndex");
    if (!preind) { std::cerr << "Unable to open mergedPreIndex.txt"; exit(1); }
    std::ofstream index("index.txt", std::ios::binary | std::ios::in | std::ios::out);
    std::ofstream metaData("metaData.txt",  std::ios::binary | std::ios::in | std::ios::out);
    std::ofstream blockLocation("blockLocation.txt");
    std::ofstream lexiconFile("lexicon.txt");
    if (!index.is_open()) {
        // File didn't exist, create it
        std::ofstream temp("index.txt", std::ios::binary);
        temp.close();
        index.open("index.txt", std::ios::binary | std::ios::in | std::ios::out);
    }
    if (!metaData.is_open()) {
        // File didn't exist, create it
        std::ofstream temp("metaData.txt", std::ios::binary);
        temp.close();
        metaData.open("metaData.txt", std::ios::binary | std::ios::in | std::ios::out);
    }
    if (!index || !metaData || !blockLocation || !lexiconFile) 
    { std::cerr << "Unable to open an output stream, check what files are missing\n"; exit(1); }

    int count = 0;
    uint32_t freq;
    uint64_t packedNum;

    std::vector<std::string> termToWord;
    readVector(termToWord); 
    std::unordered_map<uint32_t, lexiconData> lexicon; 
    uint32_t currChunk = 0;
    Block bufferBlock = Block(&index, &metaData, &blockLocation);
    cout << termToWord[1244] << endl;
    uint32_t prevTermID = 0;
    // uint32_t termCount = 0;
    uint32_t termid;
    uint32_t docid;
    uint32_t pageLoc = 0;
    std::vector<std::pair<uint32_t, int>> tempSkip_lastDocID; // first is the skip to next, second is the last docid at that skip
    while (true){ 
        preind >> packedNum; // get the packed num
        preind >> freq; // next is the freq
        
        termid = unpackTermID(packedNum);
        docid = unpackDocID(packedNum);
        
        if (count == 0){
            std::vector<std::pair<uint32_t, int>> nextSkip_lastDocID;
            nextSkip_lastDocID.push_back({0, 0});
            lexicon[termid] = lexiconData{0, 0, 0, 0, 0, 0, 0, nextSkip_lastDocID}; // set up first 
            // entry into the lexicon
        }
        if (termid == 1244){
            cout << docid << "," << freq << endl;
        }

        
    
        if (prevTermID != termid){
            // cout << "Processing termid: " << termid << " docid: " << docid << " freq: " << freq << endl;

            lexicon[prevTermID].numChunks = currChunk - lexicon[prevTermID].startChunk;
            lexicon[prevTermID].elemsLastChunk = bufferBlock.currListInd - bufferBlock.flushedListInd;
            lexicon[prevTermID].lastChunkPos = bufferBlock.currListInd-1;
            if (currChunk == lexicon[prevTermID].startChunk){
                lexicon[prevTermID].elemsFirstChunk = bufferBlock.currListInd - bufferBlock.flushedListInd;
            }
            bufferBlock.subtractionCompress();
            uint32_t size = bufferBlock.virtualFlush();
            lexicon[prevTermID].nextBytes_lastDocID.push_back({size, (int)docid}); // no one knows what the 
            // last docid is yet. I will trust the next termid to fill it in for me once it finds out
            
            pageLoc += invListSize(lexicon, prevTermID); // update the pageloc
            if (prevTermID == 0){
                uint32_t ploc = index.tellp();
                cout << "true: " << ploc + size + lexicon[prevTermID].elemsLastChunk << endl;
                cout << "guess: " << pageLoc << endl;
            }
            lexicon[termid] = lexiconData{static_cast<uint32_t>(index.tellp()),
                                            currChunk,
                                            0,
                                            0, 
                                            0,
                                            bufferBlock.currListInd,
                                            0,
                                            tempSkip_lastDocID};
            
        }
        prevTermID = termid;

        bufferBlock.addToChunk(docid, (uint8_t)freq);

        if (bufferBlock.currListInd == CHUNK_LIST_SIZE){ 
            // check if this is the first flush for the curr lexicon entry
            if (currChunk == lexicon[prevTermID].startChunk){
                lexicon[prevTermID].elemsFirstChunk = bufferBlock.currListInd - bufferBlock.flushedListInd;
            }
            bufferBlock.subtractionCompress();
            uint32_t size = bufferBlock.flush(termid);
            lexicon[prevTermID].nextBytes_lastDocID.push_back({size, (int)docid});
            bufferBlock.reset(); // flushInd = 0, currInd = 0 15595716

            currChunk++;


        }
        count++;
        if (!preind){
            if (currChunk == lexicon[prevTermID].startChunk){
                lexicon[prevTermID].elemsFirstChunk = bufferBlock.currListInd - bufferBlock.flushedListInd;
            }
            lexicon[prevTermID].elemsLastChunk = bufferBlock.currListInd - bufferBlock.flushedListInd;
            lexicon[prevTermID].lastChunkPos = bufferBlock.currListInd-1;
            lexicon[prevTermID].numChunks = currChunk - lexicon[prevTermID].startChunk;

            bufferBlock.subtractionCompress();
            
            uint32_t size = bufferBlock.flush(termid);
            lexicon[prevTermID].nextBytes_lastDocID.push_back({size, (int)docid});

            // bufferBlock.flushMetaData(9); 
            lexicon[prevTermID].numChunks = currChunk - lexicon[prevTermID].startChunk;

            break;
        }

    }
    writeLex(lexiconFile, lexicon, termToWord);
    cout << invListSize(lexicon, 0) << endl;
    printLex(lexicon, 1);
    printLex(lexicon, prevTermID);

}




/*
Returns the first 32 bits of the packed number, which is the termID 8841823
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

// void printArr(uint32_t* arr, int size){
//     for (int i=0;i<size;i++){

//         cout << arr[i] << " ";
//     }
//     cout << endl;

// }

void readVector(std::vector<std::string>& words) {
    std::ifstream termToWord("tempFiles/termToWord");
    if (!termToWord) { std::cerr << "Failed to open termToWord\n"; exit(1); }

    std::string holder;
    while (termToWord >> holder) { words.push_back(holder); }
    termToWord.close();
}

void writeLex(std::ofstream& lexFile, std::unordered_map<uint32_t, lexiconData> lexicon, vector<string>& termtoWord) {
    for (const auto& [term, data] : lexicon) {
        lexFile << termtoWord[term] << " " << data.startByte << " " << data.numChunks << " " << data.elemsFirstChunk << " " << data.elemsLastChunk << " " << data.firstChunkPos << " " << data.lastChunkPos << " ";
        for (const auto& pair : data.nextBytes_lastDocID) {
            lexFile << pair.first << " " << pair.second << " ";
        }
        lexFile << endl;
    }
}

// you can only run this assuming the inv List is complete
uint32_t invListSize(std::unordered_map<uint32_t, lexiconData>& lexicon, uint32_t termid){
    lexiconData& lex = lexicon[termid];
    uint32_t total = 0;
    for (uint32_t i=0;i<lex.nextBytes_lastDocID.size();i++){
        total += lex.nextBytes_lastDocID[i].first;
        if (lex.nextBytes_lastDocID.size() == 1){
            total += lex.elemsFirstChunk;
            return total;
        }
        if (i == 0){
            total += lex.elemsFirstChunk;
        }
        else if (i == lex.nextBytes_lastDocID.size() - 1){
            total += lex.elemsLastChunk;
        }
        else {
            total += CHUNK_LIST_SIZE;
        }
    }
    return total;
}

void updateLastDocID(std::unordered_map<uint32_t, lexiconData>& lexicon, uint32_t termid, int lastDocID){
    for (int i=termid;i>=0;i--){
        lexiconData& lex = lexicon[i];
        for (int j=lex.nextBytes_lastDocID.size()-1;j>=0;j--){
            if (lex.nextBytes_lastDocID[j].second == -1){
                lex.nextBytes_lastDocID[j].second = lastDocID;
            }
            else {
                return;
            }
        }
        
    }
}   