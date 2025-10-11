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
    uint32_t currIndexSize;
    uint8_t flushedChunkInd;
    uint8_t flushedListInd;
    // the fields are the meta data
    uint32_t compressedChunkSizes[NUM_CHUNKS];
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
    uint32_t flush(); // to flush contents into a file 
    // flushes out the last docid list and the list for the compressed size of 
    // the docid lists
    void flushMetaData();
    void subtractionCompress();
    uint32_t lastDocID(uint8_t ind);

    void reset();
};