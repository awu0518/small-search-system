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
using namespace std;

uint32_t arrDifferences(uint32_t* arr, int start, int end);
uint32_t encodeNum(std::ofstream* output, uint32_t num);
void byteWrite(std::ofstream* output, uint32_t num, int size);
uint32_t encodedNumSize(uint32_t num);
Chunk::Chunk(){
    reset();
}
void Chunk::reset(){
    memset(docIDList, 0, sizeof(docIDList));
    memset(freqList, 0, sizeof(freqList));
}
void printArr(uint32_t* arr, int size){
    for (int i=0;i<size;i++){

        cout << arr[i] << " ";
    }
    cout << endl;

}

Block::Block(std::ofstream* indexFile, std::ofstream* metaFile, std::ofstream* blockLocation){
    this->indexFile = indexFile;
    this->metaFile = metaFile;
    this->blockLocation = blockLocation;
    currIndexSize = 0;
    reset();
}
    
void Block::addToChunk(uint32_t newID, uint8_t newFreq){
    chunks[currChunkInd].docIDList[currListInd] = newID; // append to docid list
    chunks[currChunkInd].freqList[currListInd] = newFreq; // append to freq list 
    currListInd++;
    if (currListInd == CHUNK_LIST_SIZE){
        lastDocIDs[currChunkInd] = newID; // record the last docid in chunk
    }
}
Chunk* Block::currChunk(){
    return &(chunks[currChunkInd]); 
}


// We are going to flush every time a chunck is complete
// A virtual flush is to see how many bytes would be flushed if we were to flush
// Will still rearrange flushedInd
uint32_t Block::virtualFlush(){
    uint32_t chunkBytes = 0;
    // Write docIDs
    for (int j = flushedListInd; j < currListInd; j++) {
        chunkBytes += encodeNum(indexFile, chunks[0].docIDList[j]);
    }
    flushedListInd = currListInd;
    return chunkBytes;
}
// to flush contents into a file and return how many bytes it has flushed
// This code will assume the block it is 
// flushing is not the final block (not an incomplete one)
// flush() will assume either an inverted list is being flushed with the call
uint32_t Block::flush(int num){
    uint32_t chunkBytes = 0;
    // Write docIDs
    for (int j = flushedListInd; j < CHUNK_LIST_SIZE && chunks[0].freqList[j] !=0; j++) {
        chunkBytes += encodeNum(indexFile, chunks[0].docIDList[j]);
    }
    for (int j = 0; j < CHUNK_LIST_SIZE && chunks[0].freqList[j] !=0; j++) {
        byteWrite(indexFile, chunks[0].freqList[j], sizeof(uint8_t));
    }
    flushedListInd = currListInd;
    return chunkBytes;

} 

// flushes out the last docid list and the list for the compressed size of 
// the docid lists
// Assumes that you are using this when you are flushing out a whole block
void Block::flushMetaData(int num){
    // if (num == 141885){

    //     cout << "start: " << indexFile->tellp() << endl;
    // }
    // if (num == 0){
    //     for (int i=0;i<128;i++){
    //         cout << (int)chunks[1].docIDList[i] << " ";
    //     }
    //     cout << endl;
    // }
    for (int i=0;i<NUM_CHUNKS;i++){
        byteWrite(metaFile, lastDocIDs[i], sizeof(uint32_t));
        if (num == 1600){
            cout <<  lastDocIDs[i] << " ";
        }
    }
    for (int i=0;i<NUM_CHUNKS;i++){
        
        byteWrite(metaFile, compressedDocIDSizes[i], sizeof(uint32_t));
    }
}

uint32_t Block::subtractionCompress(){
    uint32_t endVal = arrDifferences(chunks[currChunkInd].docIDList, flushedListInd, currListInd-1);
    return endVal;
}
void Block::reset(){
    currChunkInd = 0;
    currListInd = 0;
    flushedChunkInd = 0;
    flushedListInd = 0;
    memset(compressedDocIDSizes, 0, sizeof(compressedDocIDSizes));
    memset(lastDocIDs, 0, sizeof(lastDocIDs));
    for (int i=0;i<NUM_CHUNKS;i++){
        chunks[i].reset();
    }
}



uint32_t arrDifferences(uint32_t* arr, int start, int end){
    uint32_t endVal = arr[end];
    for (int i=end; i>start; i--){
        arr[i] = arr[i] - arr[i-1];
    }
    return endVal;
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
uint32_t encodedNumSize(uint32_t num) {
    uint32_t count = 1;
    while (num >= 128) {
        num = num >> 7;
        count++;
    }
    return count;
}
uint32_t encodeNum(std::ofstream* output, uint32_t num) {
    uint32_t count = 1;
    while (num >= 128) {
        uint8_t currByte = 128 + (num & 127);
        output->write(reinterpret_cast<const char*>(&currByte), sizeof(uint8_t));
        num = num >> 7;
        count++;
    }

    uint8_t last = static_cast<uint8_t>(num);
    output->write(reinterpret_cast<const char*>(&last), sizeof(uint8_t));
    return count;
}

