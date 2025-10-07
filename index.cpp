// assume 64kb blocks, lets say 10 chunks per block, chunks will contain 128 entries of docids/freq
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
using namespace std; // fuck u alex

uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void arrDifferences(uint32_t* arr, int start, int end);
void encodeNum(std::ofstream* output, uint32_t num);
void printArr(void* arr, int size);
const int CHUNK_LIST_SIZE = 128;
const int NUM_CHUNKS = 10;
class Chunk{
public:

    uint32_t docIDList[CHUNK_LIST_SIZE];
    uint8_t freqList[CHUNK_LIST_SIZE];
    // might be nice to have. we might do the compression in a function outside of the classs
    // Then again we could also do it in the class but idk
    Chunk(){
        reset();
    }
    void reset(){
        memset(docIDList, 0, sizeof(docIDList));
        memset(freqList, 0, sizeof(freqList));
    }
};

class Block {
    public:
    // the fields are the meta data
    uint32_t lastDocID[NUM_CHUNKS];

    Chunk chunks[NUM_CHUNKS];
    int currChunkInd; // keep track of which chunk we at
    int currListInd; // which ind we are in the list of each chunk
    std::ofstream* indexFile;
    std::ofstream* metaFile;

    Block(std::ofstream* indexFile, std::ofstream* metaFile){
        this->indexFile = indexFile;
        this->metaFile = metaFile;
        reset();
    }
        
    uint32_t addToChunk(uint32_t newID, uint8_t newFreq){
        chunks[currChunkInd].docIDList[currListInd] = newID; // append to docid list
        chunks[currChunkInd].freqList[currListInd] = newFreq; // append to freq list 
        currListInd++;
        if (currListInd == CHUNK_LIST_SIZE){
            lastDocID[currChunkInd] = newID; // record the last docid in chunk
            arrDifferences(chunks[currChunkInd].docIDList, 0, 128); // the substraction thing on the docids
            currChunkInd++;
            currListInd = 0;
        }
        if (currChunkInd == 10){ // need some way to update metadata for the flush when incomplete block
            flush(); // putting this here before changing currChunkInd = 0
            // the idea is that I can call flush() in main() after reading ends from index file
            // to flush out an incomplete block
            reset();
            return true;
        }
        return false;
    }
    Chunk* currChunk(){
        return &(chunks[currChunkInd]);
    }
    void flush(){// to flush contents into a file
        for (int i=0;i<currChunkInd;i++){
            for (int j=0;j < CHUNK_LIST_SIZE && chunks[i].freqList[j] != 0; j++){
                // chunks[i].freqList[j] != 0 becuase a freq can never be 0. If we reach this point in the list
                // it means the curr pos in the list hasnt been written to 
                encodeNum(indexFile, chunks[i].docIDList[j]);
            }
            for (int j=0;j < CHUNK_LIST_SIZE && chunks[i].freqList[j] != 0; j++){
                indexFile->write(reinterpret_cast<const char*>(&chunks[i].freqList[j]), sizeof(uint8_t));
            }
        }
        flushMetaData();

    } 
    void flushMetaData(){
        for (int i=0;i<NUM_CHUNKS;i++){
            *metaFile << lastDocID[i] << " ";
        }
        *metaFile << std::endl;
    }
    void reset(){
        currChunkInd = 0;
        currListInd = 0;
        memset(lastDocID, 0, sizeof(lastDocID));
        for (int i=0;i<NUM_CHUNKS;i++){
            chunks[i].reset();
        }
    }
};

int main() {
    std::ifstream preind("mergedPreIndex");
    if (!preind) { std::cerr << "Unable to open mergedPreIndex.txt"; exit(1); }
    std::ofstream index("index.txt");
    std::ofstream metaData("metaData.txt");

    int count = 0;
    uint32_t freq;
    uint64_t packedNum;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> lexicon; // will map termid to [start block, end block]
    std::vector<uint32_t> blockLocations; // will keep track how many bytes block i is from the begining 
    uint32_t startBlock = 0;
    uint32_t currBlock = 0;

    Block bufferBlock = Block(&index, &metaData);

    uint32_t prevTermID = 0;
    while (count < 150){ 
        preind >> packedNum; // get the packed num
        preind >> freq; // next is the freq

        uint32_t termid;
        uint32_t docid;
        termid = unpackTermID(packedNum);
        docid = unpackDocID(packedNum);
        
        cout << termid << " " << docid << " " << freq << endl;

        
        
        if (prevTermID != termid){
            // make the docid list the diff of the numbers for compression
            // and get sum of all stuff
            lexicon[prevTermID].first = startBlock;
            lexicon[prevTermID].second = currBlock;
            startBlock = currBlock;
        }
        if (bufferBlock.addToChunk(docid, (uint8_t)freq)){
            currBlock++;
        }
        
        prevTermID = termid;
        count++;
    }
    cout << bufferBlock.currListInd << endl;
    printArr(bufferBlock.currChunk()->docIDList, 128);
    // printArr(bufferBlock.chunks[0].freqList, 128);
    for (int i=0;i<128;i++){
        cout << (int)bufferBlock.currChunk()->freqList[i] << " ";
    }
    cout << endl;
    bufferBlock.currChunkInd++;
    bufferBlock.flush();
    
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

void printArr(void* arr, int size){
    for (int i=0;i<size;i++){
        cout << ((int*)arr)[i] << " ";
    }
    cout << endl;

}

void arrDifferences(uint32_t* arr, int start, int end){
    for (int i=end; i=start; i--){
        arr[i] = arr[i] - arr[i-1];
    }
}

/*
Writes a number compressed using varbyte as bytes into an output stream

If a number is greater than 127, we write the upper bytes with a 1 in the first bit to indicate
the number continues into the next byte, and write the remaining 7 bits of that first byte.

At the end its guaranteed to fit within a 7 bit number
*/
void encodeNum(std::ofstream* output, uint32_t num) {
    while (num >= 128) {
        uint8_t currByte = 128 + (num & 127);
        output->write(reinterpret_cast<const char*>(&currByte), sizeof(uint8_t));
        num = num >> 7;
    }

    uint8_t last = static_cast<uint8_t>(num);
    output->write(reinterpret_cast<const char*>(&last), sizeof(uint8_t));
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
