#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <limits>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <cstring>

const double K1 = 1.2;
const double B = 0.75;
const int N = 8841823;
const double DAVG = 55.9158;
const uint8_t CHUNK_SIZE = 128;
const uint8_t NUM_CHUNKS = 10;
struct lexiconData {
    uint32_t termid;
    uint32_t startBlockNum;
    uint32_t endBlockNum;
    uint32_t startChunkNum;
    uint32_t endChunkNum;
    uint32_t startChunkPos;
    uint32_t endChunkPos;

    uint32_t listLen;
    uint32_t startByte;
    uint32_t endByte;
};

struct Chunk {
    std::vector<uint8_t> compressedDocIds;
    uint8_t freq[CHUNK_SIZE];
    uint8_t finalInd;
    Chunk(){
        memset(freq, 0, sizeof(freq));
        finalInd = 0;
    }
}; 

struct UncompressedChunk {
    uint32_t docIds[CHUNK_SIZE];
    uint8_t freq[CHUNK_SIZE];
    uint8_t currPos = 0; // This will point to the exact last entry in the arrs. 
    // so if currPos = 4, there are 5 elems in the list and freq[currPos] != 0
    UncompressedChunk(){
        memset(docIds, 0, sizeof(docIds));
        memset(freq, 0, sizeof(freq));
    }
};

uint32_t decodeNum(const std::vector<uint8_t>& bytes, size_t& currPos);

struct InvertedList {
    std::vector<uint32_t> lastDocIds;
    std::vector<uint32_t> docIdBytes;
    std::vector<Chunk*> compressedChunks;
    uint32_t numDocs;
    uint8_t startPositionFirst;
    uint32_t currChunk = 0;
    UncompressedChunk* currUncompressedChunk = nullptr;
    uint8_t lastChunkLen = 0;

    ~InvertedList() {
        for (Chunk* c : compressedChunks) {
            if (c != nullptr) delete c;
        }
        if (currUncompressedChunk != nullptr) delete currUncompressedChunk;
    }
    void uncompressChunk(int chunkNum) {
        if (currUncompressedChunk != nullptr) delete currUncompressedChunk;
        currUncompressedChunk = new UncompressedChunk();
        size_t index = 0;
        for (int numDecoded = 0; numDecoded < CHUNK_SIZE; numDecoded++) {
            currUncompressedChunk->docIds[numDecoded] = decodeNum(compressedChunks[chunkNum]->compressedDocIds, index);
            currUncompressedChunk->freq[numDecoded] = compressedChunks[chunkNum]->freq[numDecoded];
            currUncompressedChunk->currPos = numDecoded;
        }
        for (int i=1;i<currUncompressedChunk->currPos+1;i++){
            currUncompressedChunk->docIds[i] += currUncompressedChunk->docIds[i-1];
        }
    }

};

void readPageTable(std::unordered_map<uint32_t, uint16_t>&);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);
double bm25(uint32_t ft, uint8_t fdt, uint16_t docLen);
uint32_t findNextDocID(InvertedList& currList, uint32_t target);
void conjunctiveDAAT();
void disjunctiveDAAT();
void loadLexicon(std::vector<lexiconData>& lexicons, std::ifstream& lexFile);
void loadMetaData(uint32_t start, uint32_t end, uint32_t,  std::vector<uint32_t>& lastDocIds, std::vector<uint32_t>& docIdBytes);
void getInvertedIndex(int termid, std::vector<lexiconData>& lexicons, InvertedList& invList);

using namespace std;
int main() {
    std::ifstream index("index.txt");
    std::ifstream lexFile("lexicon.txt");
    std::unordered_map<uint32_t, uint16_t> pageTable;
    // readPageTable(pageTable);
    std::vector<lexiconData> lexicons;

    InvertedList invList;
    loadLexicon(lexicons, lexFile);

// 1471949
    int ind = 49;
    for (int i=ind;i<ind+1;i++){
    cout << "len: " <<  lexicons[i].listLen <<  " " << lexicons[i].startBlockNum << " " <<  lexicons[i].endBlockNum << " " << lexicons[i].endByte - lexicons[i].startByte 
    << " " << lexicons[i].startBlockNum << endl;
    }
    getInvertedIndex(ind, lexicons, invList);
    cout << invList.lastDocIds.size() << endl;
    cout << invList.docIdBytes.size() << endl;
    cout << invList.compressedChunks.size() << endl;   
    cout << invList.compressedChunks.back()->compressedDocIds.size() << endl;   
    int byteTotal = 0;
    for (int i=0;i<invList.docIdBytes.size();i++){
        cout << invList.docIdBytes[i] << " ";
        byteTotal += invList.docIdBytes[i];
    }
    cout << endl;
    cout << "byte total: " << byteTotal << endl;
    // for (int i=0;i<lexicons.size();i++){
    //     getInvertedIndex(i, lexicons, invList);
    //     int total = 0;
    //     for (int i=0;i<invList.compressedChunks.size();i++){
    //         total += invList.compressedChunks[i]->compressedDocIds.size();
    //         for (int j=0;j<128 && invList.compressedChunks[i]->freq[j] != 0;j++){
    //             total++;
    //         }
    //     }
    //     if (total != lexicons[i].endByte - lexicons[i].startByte){
    //         cout << "mismatch " << i << " " << total << " " << lexicons[i].endByte - lexicons[i].startByte << endl;
    //     }
    // }
    int total = 0;
    for (int i=0;i<invList.compressedChunks.size();i++){
        total += invList.compressedChunks[i]->compressedDocIds.size();
        for (int j=0;j<invList.compressedChunks[i]->finalInd;j++){
            total++;
        }
    }
    cout << "what we got: " << total << endl;
    cout << "true size: "<<  lexicons[ind].endByte - lexicons[ind].startByte << endl;
    // invList.uncompressChunk(0);
    // for (int i=0;i<128;i++){
    //     cout << invList.currUncompressedChunk->docIds[i] << " ";
    // }
}

uint32_t decodeNum(const std::vector<uint8_t>& bytes, size_t& currPos) {
    uint32_t num = 0;
    uint32_t shift = 0;
    uint8_t currByte;

    while ((currByte = static_cast<uint8_t>(bytes[currPos++])) >= 128) {
        num = num + ((currByte & 127) << shift);
        shift += 7;
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
        delete currList.currUncompressedChunk;

        currList.currUncompressedChunk = new UncompressedChunk{};
        size_t index = 0;
        for (int i = 0; i < CHUNK_SIZE; i++) {
            currList.currUncompressedChunk->docIds[i] = decodeNum(currList.compressedChunks[currChunk]->compressedDocIds, index);
            currList.currUncompressedChunk->freq[i] = currList.compressedChunks[currChunk]->freq[i];
        }
    }

    for (int i = 0; i < CHUNK_SIZE; i++) { 
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

void loadLexicon(std::vector<lexiconData>& lexicons, std::ifstream& lexFile){
    lexiconData tempLex;
    while (lexFile){
        lexFile >> tempLex.termid >> tempLex.startBlockNum >>
        tempLex.endBlockNum >> tempLex.startChunkNum >> tempLex.endChunkNum >> tempLex.startChunkPos >> tempLex.endChunkPos >>
        tempLex.listLen >> tempLex.startByte >> tempLex.endByte;

        lexicons.push_back(tempLex);
    }
    std::sort(lexicons.begin(), lexicons.end(),
        [](const lexiconData& a, const lexiconData& b) {
            return a.termid < b.termid;
        }
    );
}

void getInvertedIndex(int termid, std::vector<lexiconData>& lexicons, InvertedList& invList){
    std::ifstream indexFile("index.txt", std::ios::binary);
    if (!indexFile.is_open()) {
        std::cerr << "Unable to open index file\n";
        exit(1);
    }
    lexiconData lex = lexicons[termid];
    loadMetaData(lex.startBlockNum, lex.endBlockNum, lex.startChunkNum, invList.lastDocIds, invList.docIdBytes);

    indexFile.seekg(lex.startByte, std::ios::beg);
    
    Chunk* tempChunk = new Chunk();
    int currChunk = lex.startChunkNum;
    uint8_t tempNum;
    uint32_t totalBytesRead = 0;
    uint32_t maxBytes = lex.endByte - lex.startByte; 
    // uint32_t maxBytes = 356;

    cout << "start size: " <<  invList.docIdBytes[currChunk] << endl;
    uint32_t endPos = (currChunk == lex.endChunkNum) ? lex.listLen: invList.docIdBytes[currChunk];
    uint32_t startPos = (currChunk == lex.startChunkNum) ? 0 : ;
    for (uint32_t i=0;i<endPos;i++){
        indexFile.read(reinterpret_cast<char*>(&tempNum), sizeof(uint8_t));

        tempChunk->compressedDocIds.push_back(tempNum);
        totalBytesRead++;
    }
    endPos = (currChunk == lex.endChunkNum) ? lex.endChunkPos : CHUNK_SIZE;
    for (uint8_t i=lex.startChunkPos;i<endPos;i++){
        indexFile.read(reinterpret_cast<char*>(&tempNum), sizeof(uint8_t));

        tempChunk->freq[i] = tempNum;
        totalBytesRead++;
        tempChunk->finalInd = i+1;

    }
    invList.compressedChunks.push_back(tempChunk);
    currChunk++;
    while (true){
        // cout << "sdfsdf size: " <<invList.docIdBytes[currChunk] << endl; 50774
        if (totalBytesRead >= maxBytes || !indexFile){ 
            return;
        }
        tempChunk = new Chunk();
        for (uint32_t i=0;i<invList.docIdBytes[currChunk];i++){
            indexFile.read(reinterpret_cast<char*>(&tempNum), sizeof(uint8_t));
            tempChunk->compressedDocIds.push_back(tempNum);
            totalBytesRead++;
            cout << "total read: " << totalBytesRead << " curr: " << i << " out of " << invList.docIdBytes[currChunk] << endl;   
        }
        for (uint8_t i=0;i<CHUNK_SIZE;i++){
            indexFile.read(reinterpret_cast<char*>(&tempNum), sizeof(uint8_t));
            tempChunk->freq[i] = tempNum;
            tempChunk->finalInd = i+1;
            totalBytesRead++;
            if (totalBytesRead == maxBytes || !indexFile){
                invList.compressedChunks.push_back(tempChunk);
                return;
            }
            if (totalBytesRead > maxBytes){
                std::cout << "this is wrong" << std::endl;
                return;
            }
        }
        invList.compressedChunks.push_back(tempChunk);
        currChunk++;
    }
    
}   

void loadMetaData(uint32_t start, uint32_t end, uint32_t chunkNum, 
                  std::vector<uint32_t>& lastDocIds,
                  std::vector<uint32_t>& docIdBytes) {

    std::ifstream metaFile("metaData.txt", std::ios::binary);
    if (!metaFile.is_open()) {
        std::cerr << "Unable to open meta data file\n";
        exit(1);
    }
    metaFile.seekg(static_cast<std::streamoff>(start) * 80);

    uint32_t tempVal = 0;
    for (uint32_t block = start; block <= end; ++block) {
        for (uint8_t i = 0; i < NUM_CHUNKS; ++i) {
            metaFile.read(reinterpret_cast<char*>(&tempVal), sizeof(uint32_t));
            lastDocIds.push_back(tempVal);
        }
        for (uint8_t i = 0; i < NUM_CHUNKS; ++i) {
            metaFile.read(reinterpret_cast<char*>(&tempVal), sizeof(uint32_t));
            docIdBytes.push_back(tempVal);
        }
    }
}
