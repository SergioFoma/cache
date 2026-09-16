#include "parser.hpp"

#include <fstream>
#include <iostream>
#include <istream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include "cache.hpp"

namespace cache {

// ============================== JSON PARSER =================================

namespace {
std::vector<std::pair<type, size_t>> ParseCache(nlohmann::json& json_data) {
  auto& hierarchy_config = json_data["cache"];
  if (!hierarchy_config.is_array() || hierarchy_config.empty())
    throw std::runtime_error{"Incorrect configuration format."};

  std::vector<std::pair<type, size_t>> hierarchy{};
  for (auto& cache_layer : hierarchy_config) {
    auto name_it = cache_layer.find("name");
    if ((name_it == cache_layer.end()) || !name_it->is_string())
      throw std::runtime_error{"Incorrect configuration format."};

    auto ht_name_it = kStringToEnumTable.find(name_it->get_ref<std::string&>());
    if (ht_name_it == kStringToEnumTable.end())
      throw std::runtime_error{"Incorrect cache name."};

    auto size_it = cache_layer.find("size");
    if ((size_it == cache_layer.end()) || !size_it->is_number_unsigned())
      throw std::runtime_error{"Incorrect configuration format."};
    size_t size = size_it->get<size_t>();

    std::pair<type, size_t> handled_cache_layer = {ht_name_it->second, size};
    hierarchy.push_back(std::move(handled_cache_layer));
  }

  return hierarchy;
}
}  // namespace

void ConfigJson::ReadConfig() {

  auto cache_it = json_data_.find("cache");
  if (cache_it != json_data_.end()) {
    hierarchy_ = ParseCache(json_data_);
  }
}

// ================================= TXT PARSER ===============================

void ConfigTxt::ReadConfig() {

  size_t layer_amount = 0;
  if (!(txt_file_ >> layer_amount))
    throw std::runtime_error("Incorrect cache format");
  if (!layer_amount)
    throw std::runtime_error("Layer amount can't be zero");

  for (size_t i = 0; i < layer_amount; ++i) {
    std::string tmp_str;

    if ((!(txt_file_ >> tmp_str)))
      throw std::runtime_error("Couldn't read cache name");

    auto ht_name_it = kStringToEnumTable.find(tmp_str);
    if (ht_name_it == kStringToEnumTable.end())
      throw std::runtime_error{"Incorrect cache name"};

    hierarchy_.push_back(ht_name_it->second);
  }
}

// =============================== TEST READER ================================

std::vector<int> ReadTestsData(std::istream& in) {

  size_t count = 0;
  int key = 0;
  std::vector<int> keys;

  if (!(in >> count))
    throw std::runtime_error("Incorrect test format");

  for (size_t i = 0; i < count; ++i) {
    if (!(in >> key))
      throw std::runtime_error("Incorrect test format");
    keys.push_back(key);
  }

  return keys;
}
}  // namespace cache
