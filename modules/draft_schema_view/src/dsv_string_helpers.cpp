#include "dsv_string_helpers.h"

#include "sqlite3.h"
#include <iostream>
#include <regex>
#include <sstream>


void sqlite3_free_string_list(char **stringList, int stringCount) {
  if (stringList != nullptr) {
    for (auto i = 0; i < stringCount; i++) {
      if (stringList[i] != nullptr) {
        sqlite3_free(stringList[i]);
      }
    }
    sqlite3_free(stringList);
  }
}

[[nodiscard]] std::string
join_with_comma(const std::vector<std::string_view> &vec) {
  if (vec.empty())
    return "";

  std::ostringstream oss;
  oss << vec[0]; // Add the first element

  for (size_t i = 1; i < vec.size(); ++i) {
    oss << "," << vec[i]; // Add comma before subsequent elements
  }

  return oss.str();
}
