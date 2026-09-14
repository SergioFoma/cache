#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>

namespace {
constexpr int kStartValue = 1;
constexpr int kFinalValue = 300;

void CreateTestsFile(size_t count, unsigned int seed = 0,
                     const std::string& file_name = "test.txt") {

  std::mt19937 gen(seed);
  std::uniform_int_distribution<> distrib(kStartValue, kFinalValue);
  std::ofstream data_file{file_name, std::ofstream::out};

  data_file << count << "\n";
  for (size_t val_num = kStartValue; val_num <= count; ++val_num) {
    data_file << distrib(gen) << '\n';
  }
}
}  // namespace

int main(const int argc, const char* const* argv) {

  if (argc != 4) {
    std::cout << "The number of argents is incorrect!\n";
    return 0;
  }
  size_t count{};
  unsigned int seed{};
  sscanf(argv[1], "%zu", &count);
  sscanf(argv[2], "%u", &seed);

  CreateTestsFile(count, seed, argv[3]);

  return 0;
}
