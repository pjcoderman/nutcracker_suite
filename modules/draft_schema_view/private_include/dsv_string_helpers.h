#pragma once

#include <regex>
#include <stdio.h>


/**
 * @brief Case insensitive check for whether or not a specified string contains
 * another specified string.
 *
 * @param within_str The string within which to search for search_str.
 * @param search_str The string to be found (case insensitive).
 * @return true The specified string was found.
 * @return false The specified string was not found.
 */
inline bool string_contains_ci(std::string_view within_str,
                               std::string_view search_str) noexcept {
  return std::search(within_str.begin(), within_str.end(), search_str.begin(),
                     search_str.end(), [](char a, char b) {
                       return std::toupper(static_cast<unsigned char>(a)) ==
                              std::toupper(static_cast<unsigned char>(b));
                     }) != within_str.end();
}

/**
 * @brief Frees the specified sqlite3 string list.
 *
 * @param stringList The string list to be freed.
 * @param stringCount The number of strings in the string list.
 */
void sqlite3_free_string_list(char **stringList, int stringCount);

/**
 * @brief Builds a string that is a comma-separated list of the specified
 * strings.
 *
 * @param vec A vector specifying the strings to be joined.
 * @return std::string A string that is a comma-separated list of the specified
 * strings.
 */
[[nodiscard]] std::string
join_with_comma(const std::vector<std::string_view> &vec);

/**
 * @brief Reads a string from a pointer and number of bytes.
 *
 * @param str A pointer to the string.
 * @param str_bytes  The number of bytes comprising the string (with or without
 * terminating null).
 * @return std::string The string read from the specified pointer.
 */
[[nodiscard]] inline std::string read_string(const char *str, int str_bytes) {
  std::string result;
  if (str != nullptr && str_bytes > 0) {
    return std::string(str, str + str_bytes);
  }
  return "";
}

/**
 * @brief Reads a string from a pointer and number of bytes.
 *
 * @param str A pointer to the string.
 * @param str_bytes  The number of bytes comprising the string (with or without
 * terminating null).
 * @return std::string The string read from the specified pointer.
 */
[[nodiscard]] inline std::string read_string(unsigned const char *str,
                                             int str_bytes) {
  return read_string(reinterpret_cast<const char *>(str), str_bytes);
}
