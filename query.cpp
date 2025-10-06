#include <iostream>
#include <fstream>
#include <cstdint>

// FOR DEBUGGING
#include <sstream>
#include <iomanip>
#include <bitset>

void encodeNum(std::ofstream& output, uint32_t num);
uint32_t decodeNum(std::ifstream& input);

int main() {

    uint32_t test1 = 20;
    uint32_t test2 = 150;
    uint32_t test3 = 52345;
    uint32_t test4 = 3000000;

    std::ofstream output("testVarByte", std::ios::binary);
    if (!output) { std::cerr << "could not open testVarByte for writing"; exit(1); }

    encodeNum(output, test1);
    encodeNum(output, test2);
    encodeNum(output, test3);
    encodeNum(output, test4);

    output.close();

    std::ifstream input("testVarByte");
    if (!input) { std::cerr << "could not open testVarByte for reading"; exit(1); }

    std::cout << decodeNum(input) << std::endl;
    std::cout << decodeNum(input) << std::endl;
    std::cout << decodeNum(input) << std::endl;
    std::cout << decodeNum(input) << std::endl;
    input.close();

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

void encodeNum(std::ofstream& output, uint32_t num) {
    // FOR DEBUGGING
    std::ostringstream debug;
    debug << std::hex << std::setfill('0');

    while (num >= 128) {
        uint8_t currByte = 128 + (num & 127);
        output.write(reinterpret_cast<const char*>(&currByte), sizeof(uint8_t));

        // FOR DEBUGGING
        debug << std::bitset<8>(currByte) << ' ';

        num = num >> 7;
    }

    uint8_t last = static_cast<uint8_t>(num);
    output.write(reinterpret_cast<const char*>(&last), sizeof(uint8_t));

    // FOR DEBUGGING
    debug << std::bitset<8>(last);
    std::cout << debug.str() << std::endl;
}