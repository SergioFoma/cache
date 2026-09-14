#include <iostream>

#include "parser.hpp"

int main(int argc, char** argv) {

    if (argc != 2) {
        std::cout << "The number of argents is incorrect!\n";
        return 0;
    }

    std::fstream data_file = CreateTestsData(10, argv[1]);
    std::vector<int> keys = ReadTestsData(data_file);


    return 0;
}
