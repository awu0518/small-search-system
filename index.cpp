// g++ -O3 -std=c++20 index.cpp -o index

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>

int main() {
    std::ifstream collection("collection.tsv");
    if (!collection) { std::cerr << "Unable to open collection.tsv"; exit(1); }

    std::string line = ""; 

    for (int i = 0; i < 5; i++) {
        std::getline(collection, line);
        std::cout << line << std::endl;
    }

    return 0;
}