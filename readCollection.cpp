// g++ -O3 -std=c++20 readCollection.cpp -o readCollection

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <regex>
uint64_t pack(uint32_t termID, uint32_t docID);
uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);
void writeTempFile(const std::vector<uint64_t>& buffer);

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
    for (int i = 0; i < 5; i++) {
        std::getline(collection, line);
        size_t tab = line.find('\t');

        uint32_t docId = static_cast<uint32_t>(std::stoul(line.substr(0, tab)));
        std::string passage = line.substr(tab+1);

        std::regex pattern(R"(\b[A-Za-z](?:\.[A-Za-z]){1,2}\.?\b)");
        tokenizeString(passage, tokens, pattern);
        
        for (const std::string& token : tokens) {
            if (lexicon.find(token) == lexicon.end()) {
                lexicon.insert({token, currTermID++});
                termToWord.push_back(token);
            }

            buffer.push_back(pack(lexicon[token], docId));
        }
    }

    std::sort(buffer.begin(), buffer.end());
    // writeTempFile(buffer);

    // for (uint64_t num : buffer) {
    //     uint32_t termId = unpackTermID(num);
    //     uint32_t docId = unpackDocID(num);
    //     std::cout << "(" << termToWord[termId] << ", " << docId << ")" << " ";
    // }

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

/*
Splits and normalizes the string into tokens of all lowercase words without
nonalphanumeric characters
*/
void tokenizeString(const std::string& line, std::vector<std::string>& tokens, std::regex pattern) {
    tokens.clear();
    std::string tempString;

    // TODO: work out normalizing - ex. U.S. -> u s which might not be what we want

    for (char ch : line) {
        if (isalnum(ch)) { tempString.push_back((char)tolower(ch));}
        else if (ch == '.') {tempString.push_back('.');} // we need to keep '.'
        else if (ch == ' ') { // tokenize by space
            if (tempString.size() == 0) { continue; }
            if (!std::regex_search(tempString, pattern)
                && tempString.back() == '.'){ // if this isn't a match we don't
                tempString.pop_back(); // need the '.' in it
            } 
            tokens.push_back(tempString);
            tempString.clear();
            
        }
    }
    if (!tempString.empty()){
        if (!std::regex_search(tempString, pattern)
            && tempString.back() == '.'){ 
            tempString.pop_back(); 
        }
        tokens.push_back(tempString);
        tempString.clear();
    }

}

/*
TODO: switch to actual file, currently to vector and output for testing.

Given a vector of packed integers, groups together all the numbers so we get files of
(packedNum, freq) which can be unpacked into (termID, docID, freq). 

TODO: consider impact score instead of freq -> will need size of token vector from earlier
*/
void writeTempFile(const std::vector<uint64_t>& buffer) {
    std::vector<std::pair<uint64_t, int>> output;

    int currCount = 1;
    uint64_t currNum = buffer[0];
    for (size_t i = 1; i < buffer.size(); i++) {
        if (buffer[i] == currNum)  { currCount += 1; }
        else {
            output.push_back(std::pair(currNum, currCount));
            currNum = buffer[i];
            currCount = 1;
        }
        std::cout << buffer[i] << " ";
    }
    std::cout << std::endl;

    for (const std::pair<uint64_t, int>& pair : output) {
        std::cout << unpackTermID(pair.first) << " " << unpackDocID(pair.first) << " " << pair.second << " | ";
    }
}
