// g++ -O3 -std=c++20 readCollection.cpp -o readCollection

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <regex>
#include <filesystem>

const int FILE_PER_DOC = 34539;
const int NUM_FILES = 256;

uint64_t pack(uint32_t termID, uint32_t docID);
uint32_t unpackTermID(uint64_t pack);
uint32_t unpackDocID(uint64_t pack);
void tokenizeString(const std::string& line, std::vector<std::string>& tokens);
void writeTempFile(const std::vector<uint64_t>& buffer, int iter);
void writeOtherFiles(const std::unordered_map<std::string, uint32_t>& lexicon, 
    const std::vector<std::string>& termToWord,
    const std::unordered_map<uint32_t, size_t>& pageTable);


int main() {
    std::filesystem::create_directory("tempFiles");
    std::ifstream collection("collection.tsv");
    if (!collection) { std::cerr << "Unable to open collection.tsv"; exit(1); }

    std::unordered_map<std::string, uint32_t> lexicon;
    std::vector<std::string> termToWord;
    uint32_t currTermID = 0;

    std::unordered_map<uint32_t, size_t> pageTable;

    std::vector<uint64_t> buffer;
    std::vector<std::string> tokens;

    std::string line = ""; 
    for (int i = 0; i < NUM_FILES; i++) {
        for (int j = 0; j < FILE_PER_DOC && std::getline(collection, line); j++) {
            size_t tab = line.find('\t');

            uint32_t docId = static_cast<uint32_t>(std::stoul(line.substr(0, tab)));
            std::string passage = line.substr(tab+1);

            tokenizeString(passage, tokens);
            pageTable.insert({docId, tokens.size()});

            for (const std::string& token : tokens) {
                if (lexicon.find(token) == lexicon.end()) {
                    lexicon.insert({token, currTermID++});
                    termToWord.push_back(token);
                }

                buffer.push_back(pack(lexicon[token], docId));
                }
            }

        if (buffer.empty()) { break; }

        std::sort(buffer.begin(), buffer.end());
        writeTempFile(buffer, i);
        buffer.clear();
    }
    collection.close();

    writeOtherFiles(lexicon, termToWord, pageTable);
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
nonalphanumeric characters except those within words
*/
void tokenizeString(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    const static std::regex pattern(R"(\b[A-Za-z](?:\.[A-Za-z]){1,2}\.?\b)");
    std::string tempString;

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
Given a vector of packed integers, groups together all the numbers so we get files of
(packedNum, freq) which can be unpacked into (termID, docID, freq). 

TODO: consider impact score instead of freq -> will need size of token vector from earlier
*/
void writeTempFile(const std::vector<uint64_t>& buffer, int iter) {
    std::string fileName = "tempFiles/temp" + std::to_string(iter);
    std::ofstream output(fileName);
    if (!output) { std::cerr << "Failed to open stream for temp file"; exit(1); }

    int currCount = 1;
    uint64_t currNum = buffer[0];
    for (size_t i = 1; i < buffer.size(); i++) {
        if (buffer[i] == currNum)  { currCount += 1; }
        else {
            output << currNum << " " << currCount << " ";
            currNum = buffer[i];
            currCount = 1;
        }
    }
    output << currNum << " " << currCount << " "; // for final object
    output.close();
}

void writeOtherFiles(const std::unordered_map<std::string, uint32_t>& lexicon, 
    const std::vector<std::string>& termToWord,
    const std::unordered_map<uint32_t, size_t>& pageTable) {

    std::ofstream lexiconOutput("tempFiles/lexicon");
    if (!lexiconOutput) { std::cerr << "Failed to open stream for lexicon"; exit(1); }
    for (const auto& entry : lexicon) {
        lexiconOutput << entry.first << " " << entry.second << " ";
    }
    lexiconOutput.close();

    std::ofstream termToWordOutput("tempFiles/termToWord");
    if (!termToWordOutput) { std::cerr << "Failed to open stream for termToWord"; exit(1); }
    for (const std::string& entry : termToWord) {
        termToWordOutput << entry << " ";
    }
    termToWordOutput.close();

    std::ofstream pageTableOutput("tempFiles/pageTable");
    if (!pageTableOutput) { std::cerr << "Failed to open stream for pageTable"; exit(1); }
    for (const auto& entry : pageTable) {
        pageTableOutput << entry.first << " " << entry.second << " ";
    }
    pageTableOutput.close();
}
