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
uint16_t encodeNum(std::ofstream* output, uint32_t num);
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
    flushed = false;
    currIndexSize = 0;
    reset();
}
    
uint32_t Block::addToChunk(uint32_t newID, uint8_t newFreq){
    chunks[currChunkInd].docIDList[currListInd] = newID; // append to docid list
    chunks[currChunkInd].freqList[currListInd] = newFreq; // append to freq list 
    currListInd++;
    if (currListInd == CHUNK_LIST_SIZE){
        lastDocID[currChunkInd] = newID; // record the last docid in chunk
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
Chunk* Block::currChunk(){
    return &(chunks[currChunkInd]); 
}
// to flush contents into a file. This code will assume the block it is 
// flushing is not the final block (not an incomplete one)
void Block::flush(){// to flush contents into a file
    static uint32_t currBlock = 0;
    uint16_t totalSize = 0;
    uint16_t sizes[NUM_CHUNKS];
    for (int i=0;i<NUM_CHUNKS;i++){
        for (int j=0;j < CHUNK_LIST_SIZE; j++){
            totalSize += encodeNum(indexFile, chunks[i].docIDList[j]);
        }
        for (int j=0;j < CHUNK_LIST_SIZE; j++){
            byteWrite(indexFile, chunks[i].freqList[j], sizeof(uint8_t));
        }
        sizes[i] = totalSize;
        totalSize = 0;
    }

    *blockLocation << currBlock++ << " " << indexFile->tellp() << " ";

    flushMetaData(sizes);
    flushed = true;
} 
// flushes out the last docid list and the list for the compressed size of 
// the docid lists
void Block::flushMetaData(uint16_t sizes[NUM_CHUNKS]){
    for (int i=0;i<NUM_CHUNKS;i++){
        byteWrite(metaFile, lastDocID[i], sizeof(uint32_t));
    }
    for (int i=0;i<NUM_CHUNKS;i++){
        byteWrite(metaFile, sizes[i], sizeof(uint16_t));
    }
    *metaFile << std::endl;
}
void Block::flushLastBlock(){
    uint16_t sizes[NUM_CHUNKS];
    uint16_t totalSize = 0;
    // flush out all the complete chunks before the one we are currently on
    // The one we are curr on is the incomplete one

    for (int i=0;i<currChunkInd;i++){
        cout << "last docid" << endl;
        byteWrite(metaFile, lastDocID[i], sizeof(uint32_t));
        cout << lastDocID[i] << " ";
        for (uint32_t docid: chunks[i].docIDList){
            totalSize += encodeNum(indexFile, docid);
        }
        for (uint32_t freq: chunks[i].freqList){
            byteWrite(indexFile, freq, sizeof(uint8_t));
        }
        sizes[i] = totalSize;
        totalSize = 0;
    }
    // currListInd should tell us if there are leftovers in the curr chunk
    // and where it is. 
    // flush out the last docid and the docids in the list
    for (int i=0;i<currListInd;i++){
        totalSize += encodeNum(indexFile, chunks[currChunkInd].docIDList[i]);
        if (i==currChunkInd){
            byteWrite(metaFile, chunks[currChunkInd].docIDList[i], sizeof(uint32_t));
            break;
        }
    }
    // flush out the leftovers of the metadata
    sizes[currChunkInd] = totalSize;
    for (int i=0;i<currChunkInd;i++){
        byteWrite(metaFile, sizes[i], sizeof(uint16_t));
    }
    // flush out the freq list
    for (int i=0;i<currListInd;i++){
        byteWrite(indexFile, chunks[currChunkInd].freqList[i], sizeof(uint8_t));
    }
}
void Block::subtractionCompress(uint8_t startChunkPos, uint8_t startChunkInd){
    // when the inverted index only spans a small part of a single chunk
    if (startChunkInd == currChunkInd){
        cout <<"1" << endl;
        arrDifferences(chunks[currChunkInd].docIDList, startChunkPos, currListInd-1); 
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
    for (uint8_t i=endChunk;i>=startChunkInd;i--){
        
        uint8_t startPos = 0; // by default we assume we are going to compress
        // the whole chunk
        if (startChunkInd == i){// the only time we cannot assume that is 
            // if this chunk is where the inverted list begins (since the begining 
            // could be in the middle of the chunk)
            startPos = startChunkPos;
            cout << "sdlkfj" << endl;
        }
        arrDifferences(chunks[i].docIDList, startPos, CHUNK_LIST_SIZE-1);
        if (i==0){break;}
    }
}

void Block::reset(){
    currChunkInd = 0;
    currListInd = 0;
    memset(lastDocID, 0, sizeof(lastDocID));
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
uint16_t encodeNum(std::ofstream* output, uint32_t num) {
    uint16_t count = 1;
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
