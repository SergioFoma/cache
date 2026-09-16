#ifndef PARSER_HPP_
#define PARSER_HPP_

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "cache.hpp"

namespace cache {

std::vector<int> ReadTestsData(std::istream& in);

// ============================== JSON PARSER =================================
class ConfigJson {
 public:
  explicit ConfigJson(const std::string& file_name) {
    json_file_.open(file_name);
    if (!json_file_.is_open())
      throw std::runtime_error{"No such file.\n"};

    json_data_ = nlohmann::json::parse(json_file_);
    ReadConfig();
  };

  const std::vector<std::pair<type, size_t>>& GetCacheHierarchy() const {
    return hierarchy_;
  }

 private:
  std::ifstream json_file_;
  nlohmann::json json_data_;

  std::vector<std::pair<type, size_t>> hierarchy_;

  void ReadConfig();
};

// ================================= TXT PARSER ===============================
class ConfigTxt {
 public:
  explicit ConfigTxt(const std::string& file_name) {
    txt_file_.open(file_name);
    if (!txt_file_.is_open())
      throw std::runtime_error{"Could't read cache capacity"};

    ReadConfig();
    txt_file_ >> std::ws;
    if (!txt_file_.eof()) {
      std::cerr << "Warning : extra data in txt file.\n";
    }
  }

  // getters
  const std::vector<type>& GetCacheHierarchy() const { return hierarchy_; }

 private:
  std::ifstream txt_file_;

  std::vector<type> hierarchy_;

  void ReadConfig();
};
}  // namespace cache

#endif  // CONFIG_HPP_
