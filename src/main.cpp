#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

#include "cache_hierarchy.hpp"
#include "parser.hpp"

int main() {
  try {
    constexpr size_t kCap = 150;

    //
    // Чтение конфига: количество уровней и тип каждого кеша
    //

    cache::Config config("config/config.json");

    //
    // Видимо будет работать так (пример для одной итерации):
    // 1. считывание количества уровней и схему каждого уровня (json file)
    // 2. считывание размера кеша (размеры всех кешей равны) и количество входных данных
    // 3. создание CacheHierarchy (capacity для каждого cache, config, slow_get_page )
    // 4. Считывание всех данных
    //

    std::string incoming_line;

    while (std::getline(std::cin, incoming_line) && !incoming_line.empty()) {
      std::stringstream stream(incoming_line);

      size_t cache_capacities = 0;
      size_t data_size = 0;
      size_t key = 0;
      stream >> cache_capacities >> data_size;

      cache::CacheHierarchy<int, int> cache_hierarchy(
        cache_capacities, config, [](int key) { return key; });

      while (stream >> key) {
        cache_hierarchy.LookUpUpdate(key);
      }

      // В обычном режиме на выход только число хитов (HitCount)

      std::cout << cache_hierarchy.GetCacheHitCount() << "\n";
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
