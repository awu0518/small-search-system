#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <limits>
#include <unordered_map>
#include <cmath>

const double K1 = 1.2;
const double B = 0.75;
const int N = 8841823;
const double DAVG = 55.9158;

struct Chunk {
    std::vector<uint8_t> compressedDocIds;
    uint8_t freq[128];
}; 

struct UncompressedChunk {
    uint32_t docIds[128];
    uint8_t freq[128];
};

struct InvertedList {
    std::vector<uint32_t> lastDocIds;
    std::vector<uint32_t> docIdBytes;
    std::vector<Chunk*> compressedChunks;
    uint32_t numDocs;
    uint8_t startPositionFirst;
    uint32_t currChunk = 0;
    UncompressedChunk* currUncompressedChunk = nullptr;
};

uint32_t decodeNum(std::ifstream& input);
void readPageTable(std::unordered_map<uint32_t, uint16_t>&);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);
double bm25(uint32_t ft, uint8_t fdt, uint16_t docLen);
uint32_t findNextDocID(InvertedList& currList, uint32_t target);
void conjunctiveDAAT();
void disjunctiveDAAT();

int main() {
    std::unordered_map<uint32_t, uint16_t> pageTable;
    readPageTable(pageTable);

    std::string query; bool mode; std::vector<std::string> tokens;
    while (true) {
        std::cout << "Enter query: ";
        std::getline(std::cin, query);
        std::cout << "Enter 0 for conjuctive and 1 for disjunctive: ";
        std::cin >> mode;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        tokenizeString(query, tokens);

        if (!mode) { conjunctiveDAAT(); }
        else { disjunctiveDAAT(); }
    }

    return 0;
}

uint32_t decodeNum(std::ifstream& input) {
    uint32_t num = 0;
    uint8_t shift = 0;
    
    char c;
    input.get(c);
    uint8_t currByte;

    while ((currByte = static_cast<uint8_t>(c)) >= 128) {
        num = num + ((currByte & 127) << shift);
        shift += 7;
        input.get(c);
    }

    return num + (currByte << shift);
}

void readPageTable(std::unordered_map<uint32_t, uint16_t>& pageTable) {
    std::ifstream pageTableFile("tempFiles/pageTable");
    if (!pageTableFile) { std::cerr << "Unable to open page table file\n"; exit(1); }

    uint32_t tempDocId; uint16_t tempDocSize; uint32_t totalLength = 0;
    while (pageTableFile >> tempDocId >> tempDocSize) {
        totalLength += tempDocSize;
        pageTable.insert({tempDocId, tempDocSize});
    }

    std::cout << "Number of documents: " << pageTable.size() << std::endl;
    std::cout << "Average document length: " << (double)totalLength / pageTable.size() << std::endl;

    pageTableFile.close();
}

/*
Splits and normalizes the string into tokens of all lowercase words without
nonalphanumeric characters except those within words

TODO: remove duplicate words
*/
void tokenizeString(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    std::string tempString;
    
    for (char ch : line) {
        if (isalnum(ch)) { tempString.push_back((char)tolower(ch));}
        else { 
            if (tempString.size() == 0) { continue; }
            tokens.push_back(tempString);
            tempString.clear();
        }
    }
    if (!tempString.empty()){
        tokens.push_back(tempString);
        tempString.clear();
    }
}

double bm25(uint32_t ft, uint8_t fdt, uint16_t docLen) {
    double K = K1 * ((1-B) + B * (docLen) / DAVG);
    return std::log2((N - ft + 0.5) / (ft + 0.5)) * ((K1 + 1) * fdt) / (K + fdt);
}

uint32_t findNextDocID(InvertedList& currList, uint32_t target) {
    uint32_t currChunk = currList.currChunk;
    while (target > currList.lastDocIds[currChunk] && currChunk < currList.lastDocIds.size()) { currChunk++; }

    if (currChunk == currList.lastDocIds.size()) { return N; }

    if (currChunk != currList.currChunk) { 
        // compress current uncompressedChunk

        // uncompress new chunk
    }

    for (int i = 0; i < 128; i++) { 
        if (currList.currUncompressedChunk->docIds[i] >= target) { 
            return currList.currUncompressedChunk->docIds[i]; 
        }
    }
}

void conjunctiveDAAT() {
    std::cout << "Doing conjunctive DAAT" << std::endl;
}

void disjunctiveDAAT() {
    std::cout << "Doing disjunctive DAAT" << std::endl;
}