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

uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void arrDifferences(uint32_t* arr, int start, int end);
uint16_t encodeNum(std::ofstream* output, uint32_t num);
void printArr(uint32_t* arr, int size);
void readVector(std::vector<std::string>& words);
void byteWrite(std::ofstream* output, uint32_t num, int size);


struct lexiconData {
    uint32_t startBlockNum;
    uint8_t startChunkNum;
    uint8_t startChunkPos;
    uint32_t listLen;
    uint32_t startByte;
    uint32_t endByte;
};

int main() {
    std::ifstream preind("mergedPreIndex");
    if (!preind) { std::cerr << "Unable to open mergedPreIndex.txt"; exit(1); }
    std::ofstream index("index.txt");
    std::ofstream metaData("metaData.txt");
    std::ofstream blockLocation("blockLocation");

    if (!index || !metaData || !blockLocation) 
    { std::cerr << "Unable to open an output stream, check what files are missing\n"; exit(1); }

    int count = 0;
    uint32_t freq;
    uint64_t packedNum;

    std::vector<std::string> termToWord;
    readVector(termToWord); 
    std::unordered_map<std::string, lexiconData> lexicon; 
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
        
        cout << termid << " " << docid << " " << freq  << " " << count << endl;

        if (count == 0){
            lexicon[termToWord[termid]] = lexiconData{0, 0, 0, 0, 0, 0}; // set up first 
            // entry into the lexicon
        }
        if (prevTermID != termid){
            lexicon[termToWord[prevTermID]].listLen = termCount; // now we know how many entries the term had
            lexicon[termToWord[termid]] = lexiconData{currBlock, 
                                                    bufferBlock.currChunkInd, 
                                                    bufferBlock.currListInd, 0, 0, 0};
            termCount = 0;
            bufferBlock.subtractionCompress(
                lexicon[termToWord[prevTermID]].startChunkPos, 
                lexicon[termToWord[prevTermID]].startChunkNum
            );
        }
        bool block_flushed = bufferBlock.addToChunk(docid, (uint8_t)freq);
        if (block_flushed){ 
            // when printing out the final block check this to see if u had just printed out
            // a block. This will prevent when things are perfectly aligned and no incomplete blocks exists and for that reason you print out
            // the final block twice 
            currBlock++;
        }
        if (!block_flushed && !index){
            break;
        }
        else if (!index){

        }
        prevTermID = termid;
        termCount++;
        count++;
    }
    cout << "Stuff ended" << endl;
    cout << "curr list ind " << (int)bufferBlock.currListInd << endl;
    cout << "curr chunk ind " << (int)bufferBlock.currChunkInd << endl;
    cout << "curr block num " << currBlock << endl;
    // bufferBlock.subtractionCompress(0, 0);
    printArr(bufferBlock.chunks[0].docIDList, 128);
    printArr(bufferBlock.chunks[1].docIDList, 128);
    printArr(bufferBlock.chunks[2].docIDList, 128);
    printArr(bufferBlock.currChunk()->docIDList, 128);
    // printArr(bufferBlock.chunks[0].freqList, 128);
    // for (int i=0;i<128;i++){
    //     cout << (int)bufferBlock.currChunk()->freqList[i] << " ";
    // }
    // cout << endl;
    // bufferBlock.currChunkInd++;
    // cout << "flush "<< endl;
    // bufferBlock.flushLastBlock();
    
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

void printArr(uint32_t* arr, int size){
    for (int i=0;i<size;i++){

        cout << arr[i] << " ";
    }
    cout << endl;

}

void readVector(std::vector<std::string>& words) {
    std::ifstream termToWord("tempFiles/termToWord");
    if (!termToWord) { std::cerr << "Failed to open termToWord\n"; exit(1); }

    std::string holder;
    while (termToWord >> holder) { words.push_back(holder); }
    termToWord.close();
}

/*
while loading stuff form file:
    if termid is the same as the prev:
        save the docid and freq in lists
    else when we hit a new termid:
        make the docid list the differences of numbers 
        store the docid's in smaller bits
        calc total bytes sizes of each 
        flush into the block type datastructure



*/
