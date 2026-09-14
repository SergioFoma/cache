#include "config.hpp"

#include <fstream>
#include <iostream>
#include <istream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include "cache.hpp"

namespace cache {

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
      throw std::runtime_error{"Incorrect configuration format."};

    auto size_it = cache_layer.find("size");
    if ((size_it == cache_layer.end()) || !size_it->is_number_unsigned())
      throw std::runtime_error{"Incorrect configuration format."};
    size_t size = size_it->get<size_t>();

    std::pair<type, size_t> handled_cache_layer = {ht_name_it->second, size};
    hierarchy.push_back(std::move(handled_cache_layer));
  }

  return hierarchy;
}

bool ParseIsDump(nlohmann::json& json_data) {
  auto& dump_config = json_data["dump"];
  if (!dump_config.is_boolean())
    throw std::runtime_error{"Incorrect configuration format."};

  return dump_config.get<bool>();
}
}  // namespace

void Config::readConfig() {

  auto cache_it = json_data_.find("cache");
  if (cache_it != json_data_.end()) {
    hierarchy_ = ParseCache(json_data_);
  }

  auto dump_it = json_data_.find("dump");
  if (dump_it != json_data_.end()) {
    is_dump_enabled_ = ParseIsDump(json_data_);
  }
}

}  // namespace cache