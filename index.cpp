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
void arrDifferences(uint32_t* arr, int size);
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
        memset(docIDList, 0, sizeof(docIDList));
        memset(freqList, 0, sizeof(freqList));
    }
};

class Block {
    public:
    // the fields are the meta data
    uint32_t lastDocID[NUM_CHUNKS];
    uint32_t docIDSizes[NUM_CHUNKS]; // sizes of each list of docids
    uint32_t freqSizes[NUM_CHUNKS]; 

    Chunk chunks[NUM_CHUNKS];
    int currChunkInd; // keep track of which chunk we at
    int currListInd; // which ind we are in the list of each chunk
    std::ofstream* indexFile;
    std::ofstream* metaFile;

    Block(std::ofstream* indexFile, std::ofstream* metaFile){
        currChunkInd = 0;
        currListInd = 0;
        this->indexFile = indexFile;
        this->metaFile = metaFile;
        memset(lastDocID, 0, sizeof(lastDocID));
        memset(docIDSizes, 0, sizeof(docIDSizes));
        memset(freqSizes, 0, sizeof(freqSizes));
    }
    bool addToChunk(uint32_t newID, uint8_t newFreq,
            std::vector<uint32_t>* blockLocations){
        chunks[currChunkInd].docIDList[currListInd] = newID; // append to docid list
        chunks[currChunkInd].freqList[currListInd] = newFreq; // append to freq list 
        currListInd++;
        if (currListInd == 128){
            lastDocID[currChunkInd] = newID; // record the last docid in chunk
            docIDSizes[currChunkInd] = (128*4); // idk lets record in bytes
            freqSizes[currChunkInd] = (128);
            arrDifferences(chunks[currChunkInd].docIDList, 128); // the substraction thing on the docids
            currChunkInd++;
            currListInd = 0;
        }
        if (currChunkInd == 10){
            currChunkInd = 0;
            currListInd = 0;
            flush(blockLocations);
            return true;
        }
        return false;
    }
    Chunk* currChunk(){
        return &(chunks[currChunkInd]);
    }
    void flush(std::vector<uint32_t>* blockLocations){// to flush contents into a file
        flushMetaData();
        for (int i=0;i<10;i++){
        }
    } 
    void flushMetaData(){
        for (int i=0;i<10;i++){
            *metaFile << lastDocID[i] << " ";
        }
        for (int i=0;i<10;i++){
            *metaFile << docIDSizes[i] << " ";
        }
        for (int i=0;i<10;i++){
            *metaFile << freqSizes[i] << " ";
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
        if (bufferBlock.addToChunk(docid, (uint8_t)freq, &blockLocations)){
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

void arrDifferences(uint32_t* arr, int size){
    cout << "after: ";
    for (int i=size-1; i>1; i--){
        arr[i] = arr[i] - arr[i-1];
    }
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