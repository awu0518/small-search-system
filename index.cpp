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
uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);

void encodeNum(std::ofstream& output, uint32_t num);

class Chunk{
    public:
    std::vector<std::uint32_t> docIDList;
    std::vector<std::uint32_t> freqList;
    Chunk(){}
    // might be nice to have. we might do the compression in a function outside of the classs
    // Then again we could also do it in the class but idk
    Chunk(std::vector<std::uint32_t>& docIDs){
        for (uint32_t docid: docIDs){
            docIDList.push_back(docid);
        }
    }
};

class Block {
    public:
    // the fields are the meta data
    // we only rly need this stuff for when we print this into a file and so we easily 
    // figure it out when looping through the chunk vectors but why not make out lives
    // easy and keep them here so debugging is less hell
    std::vector<std::uint32_t> lastDocID;
    std::vector<std::uint32_t> docIDSizes;
    std::vector<std::uint32_t> freqSizes;
    std::vector<Chunk> chunks;
    Block(){
        for (int i=0;i<10;i++){
            chunks.push_back(Chunk()); // init 10 of these chunks (for now)
        }
    }
    void flush(){} // to flush contents into a file
};

using namespace std; // fuck u alex
int main() {
    std::ifstream preind("mergedPreIndex");
    if (!preind) { std::cerr << "Unable to open mergedPreIndex.txt"; exit(1); }
    int count = 0;
    uint32_t freq;
    uint64_t packedNum;
    std::vector<std::uint32_t> docid_list;
    std::vector<std::uint8_t> freq_list;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> lexicon; // will map termid to [byte start, byte end]

    uint32_t prevTermID = 0;
    while (count < 10){ 
        preind >> packedNum; // get the packed num
        preind >> freq; // next is the freq
        uint32_t termid;
        uint32_t docid;
        termid = unpackTermID(packedNum);
        docid = unpackDocID(packedNum);
        
        cout << termid << " " << docid << " " << freq << endl;

        if (prevTermID == termid){
            docid_list.push_back(docid);
            freq_list.push_back((uint8_t)freq);
        }
        else {
            // make the docid list the diff of the numbers for compression
            // and get sum of all stuff
        }
        count++;
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

/*
Writes a number compressed using varbyte as bytes into an output stream

If a number is greater than 127, we write the upper bytes with a 1 in the first bit to indicate
the number continues into the next byte, and write the remaining 7 bits of that first byte.

At the end its guaranteed to fit within a 7 bit number
*/
void encodeNum(std::ofstream& output, uint32_t num) {
    while (num >= 128) {
        uint8_t currByte = 128 + (num & 127);
        output.write(reinterpret_cast<const char*>(&currByte), sizeof(uint8_t));
        num = num >> 7;
    }

    uint8_t last = static_cast<uint8_t>(num);
    output.write(reinterpret_cast<const char*>(&last), sizeof(uint8_t));
}