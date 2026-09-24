#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cache_algorithm/belady_cache.hpp"
#include "cache_hierarchy.hpp"
#include "parser.hpp"

namespace fs = std::filesystem;

std::vector<int> ReadRequests(const fs::path& path, size_t expected) {
  std::ifstream input(path);
  size_t file_capacity = 0;
  size_t count = 0;
  if (!(input >> file_capacity >> count) || count != expected ||
      file_capacity == 0)
    throw std::runtime_error("Invalid dataset header: " + path.string());

  std::vector<int> requests(count);
  for (int& key : requests)
    if (!(input >> key))
      throw std::runtime_error("Missing request in: " + path.string());
  int extra = 0;
  if (input >> extra)
    throw std::runtime_error("Extra request in: " + path.string());
  return requests;
}

template <typename Cache>
void Measure(Cache& instance, const std::vector<int>& requests,
             size_t warmup, std::ostream& output) {
  for (size_t i = 0; i < warmup; ++i) instance.LookUpUpdate(requests[i]);
  const size_t before = instance.GetCacheMissCount();
  for (size_t i = warmup; i < requests.size(); ++i)
    instance.LookUpUpdate(requests[i]);
  const size_t misses = instance.GetCacheMissCount() - before;
  const size_t measured = requests.size() - warmup;
  output << ',' << measured - misses << ',' << misses << ','
         << std::fixed << std::setprecision(6)
         << 100.0 * static_cast<double>(measured - misses) / measured << '\n';
}

int main(int argc, char** argv) {
  try {
    if (argc != 4 && argc != 5)
      throw std::runtime_error("Usage: measure DATA_DIR CONFIG_DIR OUTPUT_CSV [KIND]");
    const fs::path data_dir(argv[1]);
    const fs::path config_dir(argv[2]);
    const fs::path output_path(argv[3]);
    const std::string kind_filter = argc == 5 ? argv[4] : "";

    std::ifstream manifest_file(data_dir / "manifest.json");
    if (!manifest_file)
      throw std::runtime_error("Cannot open manifest.json");
    const auto manifest = nlohmann::json::parse(manifest_file);
    const size_t warmup = manifest.at("warmup_requests").get<size_t>();
    const size_t measured = manifest.at("measured_requests").get<size_t>();
    std::vector<size_t> capacities{10, 50, 100, 200};

    std::vector<fs::path> config_files;
    for (const auto& entry : fs::directory_iterator(config_dir))
      if (entry.path().extension() == ".json")
        config_files.push_back(entry.path());
    std::sort(config_files.begin(), config_files.end());
    if (config_files.empty())
      throw std::runtime_error("No JSON configs in: " + config_dir.string());

    for (const auto& path : config_files) {
      cache::ConfigJson config(path.string());
      const auto& layers = config.GetCacheHierarchy();
      if (layers.size() == 1) capacities.push_back(layers.front().second);
    }
    std::sort(capacities.begin(), capacities.end());
    capacities.erase(std::unique(capacities.begin(), capacities.end()), capacities.end());

    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("Cannot write output CSV");
    output << "dataset,kind,seed,config,capacity,levels,requests,hits,misses,hit_rate_percent\n";
    const cache::loader<int, int> loader = [](const int& key) { return key; };
    size_t rows = 0;
    for (const auto& dataset : manifest.at("datasets")) {
      const std::string filename = dataset.at("file").get<std::string>();
      const std::string kind = dataset.at("kind").get<std::string>();
      if (!kind_filter.empty() && kind != kind_filter) continue;
      const int seed = dataset.at("seed").get<int>();
      const auto requests = ReadRequests(data_dir / filename, warmup + measured);

      for (const auto& config_path : config_files) {
        cache::ConfigJson config(config_path.string());
        size_t capacity = 0;
        for (const auto& layer : config.GetCacheHierarchy()) capacity += layer.second;
        cache::CacheHierarchy<int, int> instance(config, loader);
        output << filename << ',' << kind << ',' << seed << ','
               << config_path.stem().string() << ',' << capacity << ','
               << config.GetCacheHierarchy().size() << ',' << measured;
        Measure(instance, requests, warmup, output);
        ++rows;
      }

      for (size_t capacity : capacities) {
        cache::Belady<int, int> instance(capacity, loader, requests);
        output << filename << ',' << kind << ',' << seed << ','
               << "BELADY_" << capacity << ',' << capacity << ",1," << measured;
        Measure(instance, requests, warmup, output);
        ++rows;
      }
    }
    std::cerr << "Wrote " << rows << " rows to " << output_path << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
