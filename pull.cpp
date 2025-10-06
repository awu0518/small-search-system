#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <regex>
#include <filesystem>

int main() {
    std::ifstream vec("./tempFiles/termToWord");
    if (!vec) { std::cerr << "Failed to open"; exit(1); }

    std::string temp;
    std::vector<std::string> tempVec;
    while (vec >> temp) {
        tempVec.emplace_back(temp);
    }
    vec.close();

    std::cout << tempVec[4026409] << " " << tempVec.size();

}