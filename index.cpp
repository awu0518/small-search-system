// g++ -O3 -std=c++20 index.cpp -o index

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>
#include <algorithm>

uint64_t pack(uint32_t termID, uint32_t docID);
uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);

int main() {
    std::ifstream collection("collection.tsv");
    if (!collection) { std::cerr << "Unable to open collection.tsv"; exit(1); }

    std::unordered_map<std::string, uint32_t> lexicon;
    std::vector<std::string> termToWord;
    uint32_t currTermID = 0;

    std::unordered_map<uint32_t, double> pageTable;

    std::vector<uint64_t> buffer;
    std::vector<std::string> tokens;

    std::string line = ""; 
    for (int i = 0; i < 2; i++) {
        std::getline(collection, line);
        size_t tab = line.find('\t');

        uint32_t docId = static_cast<uint32_t>(std::stoul(line.substr(0, tab)));
        std::string passage = line.substr(tab+1);

        tokenizeString(passage, tokens);

        for (const std::string& token : tokens) {
            if (lexicon.find(token) == lexicon.end()) {
                lexicon.insert({token, currTermID++});
                termToWord.push_back(token);

            }

            buffer.push_back(pack(lexicon[token], docId));
        }

        std::sort(buffer.begin(), buffer.end());

        // for (uint64_t num : buffer) {
        //     uint32_t termId = unpackTermID(num);
        //     uint32_t docId = unpackDocID(num);
        //     std::cout << "(" << termToWord[termId] << ", " << docId << ")" << " ";
        // }
    }

    collection.close();
    return 0;
}

/*
Returns a 64 bit unsigned integer in the following representation:
-- 32 bits (termID) -- 32 bits (docID) --
*/
uint64_t pack(uint32_t termID, uint32_t docID) {
    return (uint64_t(termID) << 32) | docID;
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

void tokenizeString(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    std::string tempString;

    // TODO: work out normalizing - ex. U.S. -> u s which might not be what we want
    for (char ch : line) {
        if (isalnum(ch)) { tempString.push_back((char)tolower(ch)); }
        else {
            if (tempString.size() == 0) { continue; }
            tokens.push_back(tempString);
            tempString.clear();
        }
    }
}
