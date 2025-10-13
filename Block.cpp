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

void arrDifferences(uint32_t* arr, int start, int end);
uint32_t encodeNum(std::ofstream* output, uint32_t num);
void byteWrite(std::ofstream* output, uint32_t num, int size);

Chunk::Chunk(){
    reset();
}
void Chunk::reset(){
    memset(docIDList, 0, sizeof(docIDList));
    memset(freqList, 0, sizeof(freqList));
}


Block::Block(std::ofstream* indexFile, std::ofstream* metaFile, std::ofstream* blockLocation){
    this->indexFile = indexFile;
    this->metaFile = metaFile;
    this->blockLocation = blockLocation;
    currIndexSize = 0;
    reset();
}
    
uint32_t Block::addToChunk(uint32_t newID, uint8_t newFreq){
    chunks[currChunkInd].docIDList[currListInd] = newID; // append to docid list
    chunks[currChunkInd].freqList[currListInd] = newFreq; // append to freq list 
    currListInd++;
    if (currListInd == CHUNK_LIST_SIZE){
        // lastDocID[currChunkInd] = newID; // record the last docid in chunk
        currChunkInd++;
        currListInd = 0;
    }
    return false;
}
Chunk* Block::currChunk(){
    return &(chunks[currChunkInd]); 
}
// to flush contents into a file and return how many bytes it has flushed
// This code will assume the block it is 
// flushing is not the final block (not an incomplete one)
// flush() will assume either an inverted list is being flushed with the call
uint32_t Block::flush(int num){
    uint32_t totalSize = 0;
    // if (num == 1244){
    //     cout << "fci: " << (int)flushedChunkInd << " currCi: " << (int)currChunkInd << endl;
    //     cout << "fLI: " << (int)flushedListInd << " currLi: " << (int)currListInd << endl;
    // }
    // if flushedChunkInd == currCHunkInd, then flushedListInd < currListInd
    // if the 2 inds are in diff chunks then the first flushedInd can loop to 128
    if (flushedChunkInd == currChunkInd){
        for (int j=flushedListInd;j < currListInd; j++){ // assume freq[currListInd] = 0
            totalSize += encodeNum(indexFile, chunks[flushedChunkInd].docIDList[j]);
        }
        for (int j=flushedListInd;j < currListInd; j++){
            byteWrite(indexFile, chunks[flushedChunkInd].freqList[j], sizeof(uint8_t));
            totalSize++;
        }
        compressedChunkSizes[currChunkInd] += totalSize;
    }
    int end = 0;
    int start = 0;
    for (int i=flushedChunkInd+1;i<=currChunkInd;i++){
        end = i == currChunkInd ? currListInd : 128;
        start = i == flushedChunkInd ? flushedListInd : 0;
        for (int j=start;j < end; j++){ // assume freq[currListInd] = 0
            totalSize += encodeNum(indexFile, chunks[i].docIDList[j]);
        }
        for (int j=start;j < end; j++){
            byteWrite(indexFile, chunks[i].freqList[j], sizeof(uint8_t));
            totalSize++;
        }
        compressedChunkSizes[i] += totalSize;
    }


    flushedChunkInd = currChunkInd;
    flushedListInd = currListInd;
    return totalSize;

} 
uint32_t Block::lastDocID(uint8_t ind){
    uint32_t sum = 0;
    for (uint32_t docid: chunks[ind].docIDList){
        sum+=docid;
    }
    return sum;
}
// flushes out the last docid list and the list for the compressed size of 
// the docid lists
// Assumes that you are using this when you are flushing out a whole block
void Block::flushMetaData(int num){
    for (int i=0;i<NUM_CHUNKS;i++){
        if (num == 141885){
            cout <<  lastDocID(i) << " ";
        }
        byteWrite(metaFile, lastDocID(i), sizeof(uint32_t));
    }
    if (num == 141885){cout << endl;}
    for (int i=0;i<NUM_CHUNKS;i++){
        if (num == 141885){
            cout <<  compressedChunkSizes[i] << " ";
        }
        byteWrite(metaFile, compressedChunkSizes[i], sizeof(uint32_t));
    }
}

void Block::subtractionCompress(){
    // when the inverted index only spans a small part of a single chunk
    if (flushedChunkInd == currChunkInd){
        arrDifferences(chunks[currChunkInd].docIDList, flushedListInd, currListInd-1); 
        return;
    }
    uint8_t endChunk = currChunkInd;
    // if chunkListInd == 0, the prev chunks is full and the curr one is empty
    // so we can just go to that prev chunk
    if (currListInd != 0){
        // this is for the case where the list extends to the prev chunk
        // we know everything before the currListInd is part of the inverted
        // list so we can compress that and move on
        arrDifferences(chunks[currChunkInd].docIDList, 0, currListInd-1);
    }
    endChunk--; 

    // when the inverted index spans multiple chunks
    for (uint8_t i=endChunk;i>=flushedChunkInd;i--){
        
        uint8_t startPos = 0; // by default we assume we are going to compress
        // the whole chunk
        if (flushedChunkInd == i){// the only time we cannot assume that is 
            // if this chunk is where the inverted list begins (since the begining 
            // could be in the middle of the chunk)
            startPos = flushedListInd;
        }
        arrDifferences(chunks[i].docIDList, startPos, CHUNK_LIST_SIZE-1);
        if (i==0){break;}
    }
}

void Block::reset(){
    currChunkInd = 0;
    currListInd = 0;
    flushedChunkInd = 0;
    flushedListInd = 0;
    memset(compressedChunkSizes, 0, sizeof(compressedChunkSizes));
    for (int i=0;i<NUM_CHUNKS;i++){
        chunks[i].reset();
    }
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
