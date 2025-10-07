#include <iostream>
#include <vector>
#include <string>
#include <fstream>

int main() {
    std::ifstream file("tempFiles/termToWord");
    if (!file) { std::cerr << "Failed to open file"; exit(1); }
    std::vector<std::string> words;
    std::string temp;

    while (file >> temp) {
        words.emplace_back(temp);
    }

    std::cout << words[4026409] << " " << words.size();
}