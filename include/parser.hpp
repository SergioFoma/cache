#ifndef PARSER_HPP_
#define PARSER_HPP_

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <fstream>
#include <unordered_map>

#include "cache.hpp"

namespace cache {

const std::vector<std::pair<type, size_t>> kDefaultCacheConfiguration = {
    {type::kLru, 10}};

class Config {
 private:
  std::fstream json_file_;
  nlohmann::json json_data_;

  std::vector<std::pair<type, size_t>> hierarchy_{kDefaultCacheConfiguration};
  bool is_dump_enabled_{false};

  void readConfig();

 public:
  explicit Config(const std::string& file_name) {
    json_file_.open(file_name);
    if (!json_file_.is_open())
      throw std::runtime_error{"No such file.\n"};

    json_data_ = nlohmann::json::parse(json_file_);
    readConfig();
  };

  bool GetIsDumpEnabled() const { return is_dump_enabled_; }
  const std::vector<std::pair<type, size_t>>& GetCacheHierarchy() const { return hierarchy_; }
  std::fstream CreateTestsData(unsigned int seed, const char* file_name);
  std::vector<int> ReadTestsData(std::fstream& data_file);
};
}  // namespace cache

#endif  // CONFIG_HPP_
