#include <CLI/CLI.hpp>
#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "cache_algorithm/belady_cache.hpp"
#include "cache_hierarchy.hpp"
#include "parser.hpp"

const cache::loader<int, int> kPseudoLoader = [](int key) {
  return key;
};

int main(int argc, char** argv) {
  CLI::App app{"Cache task from undergraduate C++ course"};
  argv = app.ensure_utf8(argv);

//                  ,_     _
//                  |\\_,-~/
//                  / _  _ |    ,--.
//                 (  @  @ )   / ,-'
//                  \  _T_/-._( (
//                  /         `. \
//                 |         _  \ |
//                  \ \ ,  /      |
//                   || |-_\__   /
//                  ((_/`(____,-'
// Potentially we can add json parser to configure cache more "customizable".
// But for now we decided to skip this stage to satisfy requirements.
// And also we have paws instead of arms=)
  std::string config_filename = "config/config.txt";
  app.add_option("-f,--file,file", config_filename, "Config");
  CLI11_PARSE(app, argc, argv);

  try {
    cache::ConfigTxt config(config_filename);

    size_t cache_capacity = 0;
    if (!(std::cin >> cache_capacity)) {
      throw std::runtime_error("Couldn't read cache capacity");
    }

    auto vec = cache::ReadTestsData(std::cin);

    cache::CacheHierarchy<int, int> cache_hierarchy(cache_capacity, config,
                                                    kPseudoLoader);
    for (const auto& input : vec) {
      cache_hierarchy.LookUpUpdate(input);
    }
    std::cout << cache_hierarchy.GetCacheHitCount() << "\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
