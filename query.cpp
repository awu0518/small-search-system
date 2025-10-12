#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <limits>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <queue>

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
    uint8_t currPos;
};

struct InvertedList {
    std::vector<uint32_t> lastDocIds;
    std::vector<uint32_t> docIdBytes;
    std::vector<Chunk*> compressedChunks;
    uint32_t numDocs;
    uint8_t startPositionFirst;
    uint32_t currChunk = 0;
    UncompressedChunk* currUncompressedChunk = nullptr;
    uint8_t lastChunkLen = 0;
};

struct Compare {
    bool operator()(const std::pair<double, uint32_t>& a,
                    const std::pair<double, uint32_t>& b) const {
        return a.first > b.first;  // min-heap based on the double
    }
};

uint32_t decodeNum(const std::vector<uint8_t>& bytes, size_t& currPos);
void readPageTable(std::unordered_map<uint32_t, uint16_t>&);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);
double bm25(InvertedList* currList, uint16_t docLen);
uint32_t findNextDocID(InvertedList* currList, uint32_t target);
InvertedList* openInvertedList(const std::string& term);
void conjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable);
void disjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable);

int main() {
    std::ifstream index("index.txt");
    std::unordered_map<uint32_t, uint16_t> pageTable;
    readPageTable(pageTable);

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
            InvertedList* currList = openInvertedList(token);
            lists.push_back(std::pair<uint32_t, InvertedList*>(currList->numDocs, currList));
        }

        std::sort(lists.begin(), lists.end());

        if (!mode) { conjunctiveDAAT(lists, pageTable); }
        else { disjunctiveDAAT(lists, pageTable); }
    }

    // return 0;

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
    while (target > currList->lastDocIds[currChunk] && currChunk < currList->lastDocIds.size()) { currChunk++; }
    if (currChunk >= currList->lastDocIds.size()) { return N; }

    if (currChunk != currList->currChunk || !currList->currUncompressedChunk) { 
        delete currList->currUncompressedChunk;

        currList->currUncompressedChunk = new UncompressedChunk{};
        currList->currChunk = currChunk;
        Chunk* newChunk = currList->compressedChunks[currChunk];
        int chunkLen = (currChunk + 1 == currList->lastDocIds.size()) ? currList->lastChunkLen : CHUNK_SIZE;

        size_t index = 0;
        for (int i = 0; i < chunkLen; i++) {
            currList->currUncompressedChunk->docIds[i] = decodeNum(newChunk->compressedDocIds, index);
            currList->currUncompressedChunk->freq[i] = newChunk->freq[i];
        }
        for (int i = 1; i < chunkLen; i++) {
            currList->currUncompressedChunk->docIds[i] += currList->currUncompressedChunk->docIds[i - 1];
        }
    }

    int chunkLen = (currChunk + 1 == currList->lastDocIds.size()) ? currList->lastChunkLen : CHUNK_SIZE;
    for (int i = 0; i < chunkLen; i++) { 
        if (currList->currUncompressedChunk->docIds[i] >= target) { 
            currList->currUncompressedChunk->currPos = i;
            return currList->currUncompressedChunk->docIds[i]; 
        }
    }
}

InvertedList* openInvertedList(const std::string& term) {
    return nullptr;
}

void conjunctiveDAAT(std::vector<std::pair<uint32_t, InvertedList*>>& lists, 
    const std::unordered_map<uint32_t, uint16_t>& pageTable) {

    std::cout << "Doing conjunctive DAAT" << std::endl;
    InvertedList* baseList = lists[0].second;
    std::priority_queue<std::pair<double, uint32_t>, std::vector<std::pair<double, uint32_t>>, Compare> heap;

    uint32_t currDocId = 0;
    while ((currDocId = findNextDocID(baseList, currDocId)) != N) {
        size_t index = 1;
        for (; index < lists.size(); index++) {
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
    for (int i = 0; i < numEssential; i++) { 
        InvertedList* currList = lists[i].second;
        uint32_t currDocId = 0;
        for (size_t currDocIndex = 0; currDocIndex < currList->numDocs; currDocIndex++) {
            currDocId = findNextDocID(currList, currDocId);
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