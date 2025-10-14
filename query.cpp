#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <limits>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <queue>
#include <sstream>
#include <cstring>

using namespace std;
const double K1 = 1.2;
const double B = 0.75;
const int N = 8841823;
const double DAVG = 55.9158;
const uint8_t CHUNK_SIZE = 128;

struct Chunk {
    std::vector<uint8_t> compressedDocIds;
    uint8_t freq[CHUNK_SIZE];
}; 

struct UncompressedChunk {
    uint32_t docIds[CHUNK_SIZE];
    uint8_t freq[CHUNK_SIZE];
    uint8_t currPos = 0;
    UncompressedChunk(){
        memset(docIds, 0, sizeof(docIds));
        memset(freq, 0, sizeof(freq));
    }
};
uint32_t decodeNum(const std::vector<uint8_t>& bytes, size_t& currPos);
uint32_t decodeNumFromFile(std::ifstream& file);
struct InvertedList {
    std::vector<uint32_t> lastDocIds;
    std::vector<uint32_t> docIdBytes;
    std::vector<Chunk*> compressedChunks;
    uint32_t numDocs;
    uint32_t currChunk = 0;
    UncompressedChunk* currUncompressedChunk = nullptr;
    uint8_t elemsInFirstChunk;
    uint8_t elemsInLastChunk;
    void uncompressChunk(int chunkNum, uint32_t elems) {
        if (currUncompressedChunk != nullptr) delete currUncompressedChunk;
        currUncompressedChunk = new UncompressedChunk();
        size_t index = 0;
        for (int numDecoded = 0; numDecoded < elems; numDecoded++) {
            currUncompressedChunk->docIds[numDecoded] = decodeNum(compressedChunks[chunkNum]->compressedDocIds, index);
            currUncompressedChunk->freq[numDecoded] = compressedChunks[chunkNum]->freq[numDecoded];
            currUncompressedChunk->currPos = numDecoded;
        }
        for (int i=1;i<currUncompressedChunk->currPos+1;i++){
            currUncompressedChunk->docIds[i] += currUncompressedChunk->docIds[i-1];
        }
    }
};

struct LexiconInvertedList {
    std::vector<uint32_t> docIdBytes;   
    std::vector<uint32_t> lastDocIds;   
    int32_t startByte = 0;
    uint32_t numChunks = 0;
    uint32_t elemsFirstChunk = 0;
    uint32_t elemsLastChunk = 0;
    uint32_t firstChunkPos = 0;
    uint32_t lastChunkPos = 0;
};

void printLexiconEntry(const std::unordered_map<std::string, LexiconInvertedList*>& lexicon,
                       const std::string& term) 
{
    auto it = lexicon.find(term);
    if (it == lexicon.end()) {
        std::cerr << "Term '" << term << "' not found in lexicon.\n";
        return;
    }

    const LexiconInvertedList* data = it->second;

    std::cout << "Lexicon entry for term '" << term << "':\n";
    std::cout << "  startByte: " << data->startByte << "\n";
    std::cout << "  numChunks: " << data->numChunks << "\n";
    std::cout << "  elemsFirstChunk: " << data->elemsFirstChunk << "\n";
    std::cout << "  elemsLastChunk: " << data->elemsLastChunk << "\n";
    std::cout << "  firstChunkPos: " << data->firstChunkPos << "\n";
    std::cout << "  lastChunkPos: " << data->lastChunkPos << "\n";

    std::cout << "  docIdBytes: [";
    for (size_t i = 0; i < data->docIdBytes.size(); i++) {
        std::cout << data->docIdBytes[i];
        if (i + 1 < data->docIdBytes.size()) std::cout << ", ";
    }
    std::cout << "]\n";

    std::cout << "  lastDocIds: [";
    for (size_t i = 0; i < data->lastDocIds.size(); i++) {
        std::cout << data->lastDocIds[i];
        if (i + 1 < data->lastDocIds.size()) std::cout << ", ";
    }
    std::cout << "]\n";
}

struct Compare {
    bool operator()(const std::pair<double, uint32_t>& a,
                    const std::pair<double, uint32_t>& b) const {
        return a.first > b.first;  // min-heap based on the double
    }
};

void readPageTable(std::unordered_map<uint32_t, uint16_t>&);
void readLexicon(std::unordered_map<std::string, LexiconInvertedList*>&);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);
double bm25(InvertedList* currList, uint16_t docLen);
uint32_t findNextDocID(InvertedList* currList, uint32_t target);
InvertedList* openInvertedList(LexiconInvertedList*, std::ifstream&);
void conjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable);
void disjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable);

static inline int chunkLenFor(const InvertedList* L, uint32_t i) {
    if (i == 0) return L->elemsInFirstChunk;
    if (i + 1 == L->lastDocIds.size()) return L->elemsInLastChunk;
    return CHUNK_SIZE;
}

void dumpInvertedList(const InvertedList* L, std::ostream& out = std::cout) {
    if (!L) { out << "(null list)\n"; return; }
    const size_t C = L->compressedChunks.size();
    if (C == 0) { out << "(empty list)\n"; return; }

    out << "InvertedList dump:\n";
    out << "  numChunks = " << C
        << ", postings = " << L->numDocs
        << ", firstChunkLen = " << unsigned(L->elemsInFirstChunk)
        << ", lastChunkLen  = " << unsigned(L->elemsInLastChunk) << "\n";

    uint64_t totalPrinted = 0;

    for (uint32_t ci = 0; ci < C; ++ci) {
        const Chunk* ch = L->compressedChunks[ci];
        const int len = chunkLenFor(L, ci);
        const uint32_t docBytes = (ci < L->docIdBytes.size() ? L->docIdBytes[ci] : 0);

        out << "\nChunk " << ci
            << "  (len=" << len
            << ", docID-bytes=" << docBytes
            << ", lastDocId(meta)=" << (ci < L->lastDocIds.size() ? L->lastDocIds[ci] : 0)
            << ")\n";

        // Decode varbyte gaps → absolute docIDs (within this chunk)
        std::vector<uint32_t> docIds; docIds.reserve(len);
        size_t p = 0;
        for (int i = 0; i < len; ++i) {
            uint32_t gap = decodeNum(ch->compressedDocIds, p);
            uint32_t absId = (i == 0 ? gap : (docIds.back() + gap));
            docIds.push_back(absId);
        }

        // Print as (docID, freq) pairs
        out << "  postings: ";
        for (int i = 0; i < len; ++i) {
            out << "(" << docIds[i] << "," << unsigned(ch->freq[i]) << ")";
            if (i + 1 < len) out << " ";
        }
        out << "\n";

        // Sanity: decoded last should match metadata lastDocIds[ci]
        if (ci < L->lastDocIds.size() && !docIds.empty()) {
            if (docIds.back() != L->lastDocIds[ci]) {
                out << "  [WARN] lastDocId mismatch: decoded="
                    << docIds.back() << " meta=" << L->lastDocIds[ci] << "\n";
            }
        }

        totalPrinted += len;
    }

    out << "\nTotal decoded postings: " << totalPrinted << "\n";
}

int main() {
    std::ifstream index("index.txt", std::ios::binary);
    if (!index) {std::cout << "cant open index" << endl; exit(1);}
    std::unordered_map<uint32_t, uint16_t> pageTable;
    readPageTable(pageTable);
    std::unordered_map<std::string, LexiconInvertedList*> lexicon;
    readLexicon(lexicon);
    std::string query; bool mode; std::vector<std::string> tokens;
    while (true) {
        std::cout << "Enter query: ";
        std::getline(std::cin, query);
        std::cout << "Enter 0 for conjunctive and 1 for disjunctive: ";
        std::cin >> mode;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        tokenizeString(query, tokens);
        std::vector<std::pair<uint32_t, InvertedList*>> lists;

        for (const std::string& token : tokens) { 
            InvertedList* currList = openInvertedList(lexicon[token], index);
            // dumpInvertedList(currList);
            lists.push_back(std::pair<uint32_t, InvertedList*>(currList->numDocs, currList));
        }

        std::sort(lists.begin(), lists.end());

        if (!mode) { conjunctiveDAAT(lists, pageTable); }
        else { disjunctiveDAAT(lists, pageTable); }
    }
    // InvertedList* currList = openInvertedList(lexicon["triticale"], index);
    // int count = 0;
    // currList->uncompressChunk(2, lexicon["triticale"]->elemsLastChunk);
    // for (int i=0;i<currList->currUncompressedChunk->currPos;i++ ){
    //     cout << currList->currUncompressedChunk->docIds[i] <<", " << (int) (currList->currUncompressedChunk->freq[i]) << endl;
    //     count ++;
    // }
    // cout << count << endl;
    return 0;

}

uint32_t decodeNumFromFile(std::ifstream& file) {
    uint32_t num = 0;
    uint32_t shift = 0;
    uint8_t currByte = 0;

    // Keep reading bytes until a byte < 128 is found
    while (true) {
        if (!file.read(reinterpret_cast<char*>(&currByte), sizeof(uint8_t))) {
            throw std::runtime_error("Unexpected EOF while decoding number");
        }

        if (currByte >= 128) {
            num |= (currByte & 127) << shift;
            shift += 7;
        } else {
            num |= currByte << shift;
            break;
        }
    }

    return num;
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

void readLexicon(std::unordered_map<std::string, LexiconInvertedList*>& lexicon) {
    std::ifstream lexiconStream("lexicon.txt");
    if (!lexiconStream) { 
        std::cerr << "Unable to open lexicon\n"; 
        exit(1); 
    }

    std::string line;
    while (std::getline(lexiconStream, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);

        std::string word;
        uint32_t startByte, numChunks, elemsFirstChunk, elemsLastChunk, firstChunkPos, lastChunkPos;

        ss >> word >> startByte >> numChunks >> elemsFirstChunk >> elemsLastChunk >> firstChunkPos >> lastChunkPos;

        LexiconInvertedList* currLexiconEntry = new LexiconInvertedList{};
        currLexiconEntry->startByte = startByte;
        currLexiconEntry->numChunks = numChunks;
        currLexiconEntry->elemsFirstChunk = elemsFirstChunk;
        currLexiconEntry->elemsLastChunk = elemsLastChunk;
        currLexiconEntry->firstChunkPos = firstChunkPos;
        currLexiconEntry->lastChunkPos = lastChunkPos;

        // Read remaining numbers as nextBytes_lastDocID pairs
        uint32_t nextBytes;
        int lastDocID;
        while (ss >> nextBytes >> lastDocID) {
            currLexiconEntry->docIdBytes.push_back(nextBytes);
            currLexiconEntry->lastDocIds.push_back(lastDocID);
        }

        lexicon[word] = currLexiconEntry;
    }
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

    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
}

double bm25(InvertedList* currList, uint16_t docLen) {
    uint32_t ft = currList->numDocs;
    uint8_t fdt = currList->currUncompressedChunk->freq[currList->currUncompressedChunk->currPos];
    double K = K1 * ((1-B) + B * (docLen) / DAVG);
    return std::log2((N - ft + 0.5) / (ft + 0.5)) * ((K1 + 1) * fdt) / (K + fdt);
}

uint32_t findNextDocID(InvertedList* currList, uint32_t target) {
    uint32_t currChunk = currList->currChunk;
    while (currChunk < currList->lastDocIds.size() && target > currList->lastDocIds[currChunk]) { currChunk++; }
    if (currChunk >= currList->lastDocIds.size()) { return N; }
    // std::cout << "In findNextDocID, current chunk is " << currChunk << " for target " << target << std::endl;

    if (currChunk != currList->currChunk || !currList->currUncompressedChunk) { 
        delete currList->currUncompressedChunk;

        currList->currUncompressedChunk = new UncompressedChunk{};
        currList->currChunk = currChunk;
        Chunk* newChunk = currList->compressedChunks[currChunk];
        
        int chunkLen;
        if (currChunk == 0)
            chunkLen = currList->elemsInFirstChunk;
        else if (currChunk + 1 == currList->lastDocIds.size())
            chunkLen = currList->elemsInLastChunk;
        else
            chunkLen = CHUNK_SIZE;

        size_t index = 0;
        for (int i = 0; i < chunkLen; i++) {
            currList->currUncompressedChunk->docIds[i] = decodeNum(newChunk->compressedDocIds, index);
            currList->currUncompressedChunk->freq[i] = newChunk->freq[i];
        }
        for (int i = 1; i < chunkLen; i++) {
            currList->currUncompressedChunk->docIds[i] += currList->currUncompressedChunk->docIds[i - 1];
        }
        
    }

    int chunkLen;
    if (currChunk == 0)
        chunkLen = currList->elemsInFirstChunk;
    else if (currChunk + 1 == currList->lastDocIds.size())
        chunkLen = currList->elemsInLastChunk;
    else
        chunkLen = CHUNK_SIZE;
    for (int i = 0; i < chunkLen; i++) { 
        if (currList->currUncompressedChunk->docIds[i] >= target) { 
            currList->currUncompressedChunk->currPos = i;
            // std::cout << "Found docID " << currList->currUncompressedChunk->docIds[i] << std::endl;
            return currList->currUncompressedChunk->docIds[i]; 
        }
    }

    currList->currChunk++;
    return findNextDocID(currList, target);
}

static inline uint32_t elemsInChunk(const LexiconInvertedList* e, uint32_t i) {
    if (i == 0) return e->elemsFirstChunk;
    if (i + 1 == e->docIdBytes.size()) return e->elemsLastChunk;
    return CHUNK_SIZE;
}

static inline uint32_t totalPostings(const LexiconInvertedList* e) {
    if (e->docIdBytes.size() == 1) return e->elemsFirstChunk;
    return e->elemsFirstChunk + (e->docIdBytes.size() - 2) * CHUNK_SIZE + e->elemsLastChunk;
}

void skipElems(std::ifstream& index, uint32_t numElems){
    for (int i=0;i<numElems;i++){
        decodeNumFromFile(index);
    }
}

InvertedList* openInvertedList(LexiconInvertedList* lexiconMetadata, std::ifstream& index) {
    InvertedList* currList = new InvertedList{};

    currList->docIdBytes = lexiconMetadata->docIdBytes;
    currList->lastDocIds = lexiconMetadata->lastDocIds;
    currList->elemsInFirstChunk = lexiconMetadata->elemsFirstChunk;
    currList->elemsInLastChunk = lexiconMetadata->elemsLastChunk;
    // currList->firstChunkPos = firstChunkPos;
    // currList->lastChunkPos = lastChunkPos;
    currList->numDocs = totalPostings(lexiconMetadata);

    index.seekg(static_cast<std::streamoff>(lexiconMetadata->startByte), std::ios::beg);
    for (uint32_t i=0;i<lexiconMetadata->docIdBytes.size();i++){
        Chunk* chunk = new Chunk{};
        chunk->compressedDocIds.resize(currList->docIdBytes[i]); 

        index.read(reinterpret_cast<char*>(chunk->compressedDocIds.data()), static_cast<std::streamsize>(currList->docIdBytes[i]));
        if (i==0){
            skipElems(index, CHUNK_SIZE-(lexiconMetadata->elemsFirstChunk + lexiconMetadata->firstChunkPos));
            index.seekg(static_cast<std::streamoff>(index.tellg()) + lexiconMetadata->firstChunkPos, std::ios::beg);
        }
        else if (i == lexiconMetadata->docIdBytes.size()-1){
            skipElems(index, CHUNK_SIZE-(lexiconMetadata->elemsFirstChunk));

        }
        index.read(reinterpret_cast<char*>(chunk->freq), static_cast<std::streamsize>(elemsInChunk(lexiconMetadata, i)));

        
        currList->compressedChunks.push_back(chunk);
    }

    return currList;
}

void conjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable) {

    std::cout << "Doing conjunctive DAAT" << std::endl;
    InvertedList* baseList = lists[0].second;
    std::cout << "Base List contains " << baseList->numDocs << " documents.\n";
    std::priority_queue<std::pair<double, uint32_t>, std::vector<std::pair<double, uint32_t>>, Compare> heap;

    uint32_t currDocId = 0;
    while ((currDocId = findNextDocID(baseList, currDocId)) != N) {
        size_t index = 1;
        // std::cout << "Attempting search on docID: " << currDocId << std::endl;
        for (; index < lists.size(); index++) {
            // std::cout << "Attempting inner search on list: " << index << std::endl;
            if (findNextDocID(lists[index].second, currDocId) != currDocId) { break; }
        }
        
        if (index == lists.size()) {
            double impactScore = 0;
            for (size_t j = 0; j < lists.size(); j++) {
                InvertedList* currList = lists[j].second;
                impactScore += bm25(currList, pageTable.at(currDocId)); 
            }

            if (heap.size() != 10) { heap.push(std::pair<double, uint32_t>(impactScore, currDocId)); }
            else if (heap.top().first < impactScore) {
                heap.pop();
                heap.push(std::pair<double, uint32_t>(impactScore, currDocId)); 
                
            }
        }

        currDocId++;
    }

    std::vector<std::pair<double, uint32_t>> topSearches;
    while (!heap.empty()) {
        topSearches.push_back(heap.top());
        heap.pop();
    }
    
    for (size_t i = topSearches.size(); i > 0; i--) {
        std::cout << "Impact Score: " << topSearches[i-1].first << " DocID: " << topSearches[i-1].second << std::endl;
    }
}

void disjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable) {

    std::cout << "Doing disjunctive DAAT" << std::endl;
    std::priority_queue<std::pair<double, uint32_t>, std::vector<std::pair<double, uint32_t>>, Compare> heap;

    const size_t numEssential = std::max<size_t>(size_t(lists.size() * 0.3), 1);

    std::vector<std::pair<uint32_t, double>> essentialDocIds;
    for (size_t i = 0; i < numEssential; i++) { 
        InvertedList* currList = lists[i].second;
        uint32_t currDocId = 0;
        for (size_t currDocIndex = 0; currDocIndex < currList->numDocs; currDocIndex++) {
            currDocId = findNextDocID(currList, currDocId);
            if (currDocId == N) break;
            essentialDocIds.push_back(std::pair<uint32_t, double>(currDocId, bm25(currList, pageTable.at(currDocId))));
            currDocId++;
            
        }
    }

    std::sort(essentialDocIds.begin(), essentialDocIds.end());
    std::vector<std::pair<uint32_t, double>> essentialDocIdsNoDup;

    for (size_t i = 1; i < essentialDocIds.size(); i++) {
        if (essentialDocIds[i-1].first == essentialDocIds[i].first) {
            essentialDocIds[i].second += essentialDocIds[i-1].second;
        }
        else {
            essentialDocIdsNoDup.push_back(essentialDocIds[i-1]);
        }
    }
    essentialDocIdsNoDup.push_back(essentialDocIds[essentialDocIds.size() - 1]);

    for (size_t i = 0; i < essentialDocIdsNoDup.size(); i++) {
        uint32_t currDocId = essentialDocIdsNoDup[i].first;
        double currImpact = essentialDocIdsNoDup[i].second;

        for (size_t j = numEssential; j < lists.size(); j++) {
            if (findNextDocID(lists[j].second, currDocId) == currDocId) {
                if (currDocId == N) break;
                currImpact += bm25(lists[j].second, pageTable.at(currDocId));
            }
        }

        if (heap.size() != 10) { heap.push(std::pair<double, uint32_t>(currImpact, currDocId)); }
        else {
            std::pair<double, uint32_t> minImpact = heap.top();
            if (minImpact.first < currImpact) {
                heap.pop();
                heap.push(std::pair<double, uint32_t>(currImpact, currDocId)); 
            }
        }
    }

    std::vector<std::pair<double, uint32_t>> topSearches;
    while (!heap.empty()) {
        topSearches.push_back(heap.top());
        heap.pop();
    }
    
    for (size_t i = topSearches.size(); i > 0; i--) {
        std::cout << "Impact Score: " << topSearches[i-1].first << " DocID: " << topSearches[i-1].second << std::endl;
    }
}