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
    uint32_t startBlockNum;
    uint32_t endBlockNum;
    uint32_t startChunkNum;
    uint32_t endChunkNum;
    uint32_t startChunkPos;
    uint32_t endChunkPos;

    uint32_t listLen;
    uint32_t startByte;
    uint32_t endByte;
};

uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void arrDifferences(uint32_t* arr, int start, int end);
uint16_t encodeNum(std::ofstream* output, uint32_t num);
// void printArr(uint32_t* arr, int size);
void readVector(std::vector<std::string>& words);
void byteWrite(std::ofstream* output, uint32_t num, int size);
void writeLex(std::ofstream& lexFile, std::unordered_map<uint32_t, lexiconData> lexicon);


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
    uint32_t currBlock = 0;

    Block bufferBlock = Block(&index, &metaData, &blockLocation);

    uint32_t prevTermID = 0;
    uint32_t termCount = 0;
    uint32_t termid;
    uint32_t docid;
    while (true){ 
        
        preind >> packedNum; // get the packed num
        preind >> freq; // next is the freq
        
        termid = unpackTermID(packedNum);
        docid = unpackDocID(packedNum);
        
        // cout << count << endl;
        
        if (count == 0){
            lexicon[termid] = lexiconData{0, 0, 0, 0, 0, 0, 0, 0, 0}; // set up first 
            // entry into the lexicon
        }
        
        

        
        if (prevTermID != termid){
            bufferBlock.subtractionCompress();
            uint32_t size = bufferBlock.flush(prevTermID);
            lexicon[termid] = lexiconData{currBlock, 
                                            0,
                                            bufferBlock.currChunkInd, 
                                            0,
                                            bufferBlock.currListInd, 
                                            0,
                                            0, 
                                            static_cast<uint32_t>(index.tellp()), 
                                            0};
            
            lexicon[prevTermID].listLen = termCount; // now we know how many entries the term had
            lexicon[prevTermID].endByte = index.tellp();
            lexicon[prevTermID].endBlockNum = currBlock;
            lexicon[prevTermID].endChunkNum = bufferBlock.currChunkInd;
            lexicon[prevTermID].endChunkPos = bufferBlock.currListInd;  
            termCount = 0;
        }
        bufferBlock.addToChunk(docid, (uint8_t)freq);
        if (bufferBlock.currChunkInd == NUM_CHUNKS){ 
            // when printing out the final block check this to see if u had just printed out
            // a block. This will prevent when things are perfectly aligned and no incomplete blocks exists and for that reason you print out
            // the final block twice 
            bufferBlock.subtractionCompress();
            bufferBlock.flush(termid);
            bufferBlock.flushMetaData(currBlock);
            // cout << metaData.tellp() <<endl;
            blockLocation << currBlock << " " << index.tellp() << " "; 
            bufferBlock.reset();
            currBlock++;


        }
        prevTermID = termid;
        termCount++;
        count++;
        if (!preind){
            if (bufferBlock.currListInd != 0){ // check if we are not in an empty chunk
                bufferBlock.lastDocIDs[bufferBlock.currChunkInd] = bufferBlock.currChunk()->docIDList[bufferBlock.currListInd-1];
            }
            bufferBlock.subtractionCompress();
            bufferBlock.flush(termid);
            bufferBlock.flushMetaData(currBlock); 
            lexicon[prevTermID].listLen = termCount; // now we know how many entries the term had
            lexicon[prevTermID].endByte = index.tellp();
            lexicon[prevTermID].endBlockNum = currBlock;
            lexicon[prevTermID].endChunkNum = bufferBlock.currChunkInd;
            lexicon[prevTermID].endChunkPos = bufferBlock.currListInd;
            break;
        }

    }
    termid = 1244;
    cout << endl;
    cout << lexicon[termid].listLen << " " << lexicon[termid].startByte << " " << lexicon[termid].endByte << endl;
    writeLex(lexiconFile, lexicon);
    
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

void writeLex(std::ofstream& lexFile, std::unordered_map<uint32_t, lexiconData> lexicon){
    for (const auto& [term, data] : lexicon) {
        lexFile << term << " " << data.startBlockNum << " " << data.endBlockNum << " " << data.startChunkNum << " " << data.endChunkNum 
        << " " << data.startChunkPos << " " << data.endChunkPos << " " << data.listLen << " " 
        << data.startByte << " " << data.endByte << " "; 
    }
}
