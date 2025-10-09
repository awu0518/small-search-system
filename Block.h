#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <regex>
#include <filesystem>

const int CHUNK_LIST_SIZE = 128;
const int NUM_CHUNKS = 10;

class Chunk{
public:
    uint32_t docIDList[CHUNK_LIST_SIZE];
    uint8_t freqList[CHUNK_LIST_SIZE];
    // might be nice to have. we might do the compression in a function outside of the classs
    // Then again we could also do it in the class but idk
    Chunk();
    void reset();
};

class Block {
    public:
    bool flushed;
    uint32_t currIndexSize;
    // the fields are the meta data
    uint32_t lastDocID[NUM_CHUNKS];

    Chunk chunks[NUM_CHUNKS];
    uint8_t currChunkInd; // keep track of which chunk we at
    uint8_t currListInd; // which ind we are in the list of each chunk
    std::ofstream* indexFile;
    std::ofstream* metaFile;
    std::ofstream* blockLocation;

    Block(std::ofstream* indexFile, std::ofstream* metaFile, std::ofstream* blockLocation);
        
    uint32_t addToChunk(uint32_t newID, uint8_t newFreq);
    Chunk* currChunk();
    // to flush contents into a file. This code will assume the block it is 
    // flushing is not the final block (not an incomplete one)
    void flush(); // to flush contents into a file 
    // flushes out the last docid list and the list for the compressed size of 
    // the docid lists
    void flushMetaData(uint16_t sizes[NUM_CHUNKS]);
    void flushLastBlock();
    void subtractionCompress(uint8_t startChunkPos, uint8_t startChunkInd);

    void reset();
};