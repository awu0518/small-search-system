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
void readVector(std::vector<std::string>& words);
void byteWrite(std::ofstream* output, uint32_t num, int size);
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
    uint8_t currChunkInd; // keep track of which chunk we at
    uint8_t currListInd; // which ind we are in the list of each chunk
    std::ofstream* indexFile;
    std::ofstream* metaFile;
    std::ofstream* blockLocation;

    Block(std::ofstream* indexFile, std::ofstream* metaFile, std::ofstream* blockLocation){
        this->indexFile = indexFile;
        this->metaFile = metaFile;
        this->blockLocation = blockLocation;
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
        static uint32_t currBlock = 0;
    // to flush contents into a file. This code will assume the block it is 
    // flushing is not the final block (not an incomplete one)
        for (int i=0;i<currChunkInd;i++){
            for (int j=0;j < CHUNK_LIST_SIZE; j++){
                encodeNum(indexFile, chunks[i].docIDList[j]);
            }
            for (int j=0;j < CHUNK_LIST_SIZE; j++){
                indexFile->write(reinterpret_cast<const char*>(&chunks[i].freqList[j]), sizeof(uint8_t));
            }
        }

        *blockLocation << currBlock++ << " " << indexFile->tellp() << " ";

        flushMetaData();
    } 
    // 
    void flushMetaData(){
        for (int i=0;i<NUM_CHUNKS;i++){
            byteWrite(metaFile, lastDocID[i], sizeof(uint32_t));
        }
        *metaFile << std::endl;
    }
    void flushLastBlock(){
        // flush out all the complete chunks before the one we are currently on
        // The one we are curr on is the incomplete one
        for (int i=0;i<currChunkInd;i++){
            for (uint32_t docid: lastDocID){
                byteWrite(metaFile, docid, sizeof(uint32_t));
            }
            for (uint32_t docid: chunks[i].docIDList){
                encodeNum(indexFile, docid);
            }
            for (uint32_t freq: chunks[i].freqList){
                byteWrite(indexFile, freq, sizeof(uint8_t));
            }
        }
        // currListInd should tell us if there are leftovers in the curr chunk
        // and where it is. 
        for (int i=0;i<currListInd;i++){
            encodeNum(indexFile, chunks[currChunkInd].docIDList[i]);
            if (i==currChunkInd){
                byteWrite(metaFile, chunks[currChunkInd].docIDList[i], sizeof(uint32_t));
                break;
            }
        }
        for (int i=0;i<currListInd;i++){
            byteWrite(indexFile, chunks[currChunkInd].freqList[i], sizeof(uint8_t));
        }
        // There is a scenario where blocks 0 and 1 are completely full and there
        // are no other leftovers
        // This means currChunkInd = 2 and currListInd = 0 since it would point at the beginning of the new chunk
        // where there are 0 elems in that chunk. So we just record currChunkInd-1 to push it back to 1. 
        byteWrite(metaFile, currListInd == 0 ? currChunkInd-1: currChunkInd, sizeof(uint32_t));
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

struct lexiconData {
    uint32_t blockNum;
    uint8_t chunkNum;
    uint8_t chunkPos;
    uint32_t listLen;
};

int main() {
    std::ifstream preind("mergedPreIndex");
    if (!preind) { std::cerr << "Unable to open mergedPreIndex.txt"; exit(1); }
    std::ofstream index("index.txt");
    std::ofstream metaData("metaData.txt");
    std::ofstream blockLocation("blockLocation");

    if (!index || !metaData || !blockLocation) { std::cerr << "Unable to open an output stream, check what files are missing\n"; exit(1); }

    int count = 0;
    uint32_t freq;
    uint64_t packedNum;

    std::vector<std::string> termToWord;
    readVector(termToWord); 

    std::unordered_map<std::string, lexiconData> lexicon; // maps word (string) to [start block, end block]
    uint32_t startBlock = 0;
    uint32_t currBlock = 0;

    Block bufferBlock = Block(&index, &metaData, &blockLocation);

    uint32_t prevTermID = 0;
    uint32_t termCount = 0;
    while (count < 150){ 
        preind >> packedNum; // get the packed num
        preind >> freq; // next is the freq

        uint32_t termid;
        uint32_t docid;
        termid = unpackTermID(packedNum);
        docid = unpackDocID(packedNum);
        
        cout << termid << " " << docid << " " << freq << endl;
        
        if (prevTermID != termid){
            lexicon[termToWord[prevTermID]] = lexiconData{startBlock, bufferBlock.currChunkInd, bufferBlock.currListInd, termCount};
            startBlock = currBlock;
            termCount = 0;
        }
        if (bufferBlock.addToChunk(docid, (uint8_t)freq)){ // when printing out the final block check this to see if u had just printed out
            // a block. This will prevent when things are perfectly aligned and no incomplete blocks exists and for that reason you print out
            // the final block twice 
            currBlock++;
        }
        
        prevTermID = termid;
        termCount++;
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
    for (int i=end; i>start; i--){
        arr[i] = arr[i] - arr[i-1];
    }
}

void byteWrite(std::ofstream* output, uint32_t num, int size){
    output->write(reinterpret_cast<const char*>(&num), size);
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
