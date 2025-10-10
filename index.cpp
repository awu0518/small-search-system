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
    uint32_t startChunkNum;
    uint32_t startChunkPos;
    uint32_t listLen;
    uint32_t startByte;
    uint32_t endByte;
};

int main() {
    std::ifstream preind("mergedPreIndex");
    if (!preind) { std::cerr << "Unable to open mergedPreIndex.txt"; exit(1); }
    std::ofstream index("index.txt");
    std::ofstream metaData("metaData.txt");
    std::ofstream blockLocation("blockLocation.txt");
    std::ofstream lexiconFile("lexicon.txt");
    if (!index || !metaData || !blockLocation || !lexiconFile) 
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
    uint32_t currIndexSize = 0;
    while (true){ 
        
        preind >> packedNum; // get the packed num
        preind >> freq; // next is the freq
        
        termid = unpackTermID(packedNum);
        docid = unpackDocID(packedNum);
        
        if (termid != 1244 && count == 0 ){
            continue;
        }

        // cout << termid << " " << docid << " " << freq  << " " << count << endl;
        if (count > 500){
            break;
        }
        if (count == 0){
            lexicon[termToWord[termid]] = lexiconData{0, 0, 0, 0, 0, 0}; // set up first 
            // entry into the lexicon
        }
        

        if (bufferBlock.currChunkInd == 10){ 
            // when printing out the final block check this to see if u had just printed out
            // a block. This will prevent when things are perfectly aligned and no incomplete blocks exists and for that reason you print out
            // the final block twice 
            currBlock++;
            bufferBlock.subtractionCompress();
            
            currIndexSize += bufferBlock.flush();
            bufferBlock.flushMetaData();
            blockLocation << currBlock << " " << index.tellp() << " "; 
            bufferBlock.reset();

        }
        else if (prevTermID != termid){
            
            bufferBlock.subtractionCompress();
            currIndexSize += bufferBlock.flush();
            lexiconData lex = lexicon[termToWord[prevTermID]];
            lexicon[termToWord[termid]] = lexiconData{currBlock, 
                                                    bufferBlock.currChunkInd, 
                                                    bufferBlock.currListInd, currIndexSize+1, 0, 0};
            
            lexicon[termToWord[prevTermID]].listLen = termCount; // now we know how many entries the term had
            lexicon[termToWord[prevTermID]].endByte = currIndexSize;
            
            termCount = 0;
            currIndexSize = 0;
        }
        bufferBlock.addToChunk(docid, (uint8_t)freq);
        if (!index){
            // bufferBlock.flushMetaData(); 
            exit(1);
        }
        prevTermID = termid;
        termCount++;
        count++;

    }

    writeLex(lexiconFile, lexicon);

    

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

void writeLex(std::ofstream& lexFile, std::unordered_map<std::string, lexiconData> lexicon){
    for (const auto& [term, data] : lexicon) {
        lexFile << term << " " << data.startBlockNum << " " << data.startChunkNum 
        << " " << data.startChunkPos << " " << data.listLen << " " 
        << data.startByte << " " << data.endByte << " "; 
    }
}
