#pragma once

#include <string>
#include <vector>

/**
 * @brief Encodes data as a base64 string.
 *
 * @param ptr A pointer to the data to be encoded.
 * @param length The length of the data, in bytes.
 * @return std::string The base64 string encoding of the specified data.
 */
std::string base64_encode(const uint8_t *ptr, size_t length);

/**
 * @brief Encodes the characters of a string as a base64 string.
 *
 * @param str The string to be encoded.
 * @return std::string The base64 string encoding of the specified string.
 */
inline std::string base64_encode(std::string_view str) {
  return base64_encode(reinterpret_cast<const uint8_t *>(str.data()),
                       str.size());
}

/**
 * @brief Encodes the data from the specified vector as a base64 string.
 *
 * @param vec The vector to be encoded.
 * @return std::string The base64 string encoding of the data from the vector.
 */
inline std::string base64_encode(const std::vector<uint8_t> &vec) {
  return base64_encode(vec.data(), vec.size());
}

/**
 * @brief Decodes the specified base64 encoded string to a byte vector.
 *
 * @param ptr A pointer to the base64 encoded string to be decoded.
 * @param length The length, in characters, of the base64 encoded string to be
 * decoded.
 * @return std::vector<uint8_t> A vector containing the data decoded from the
 * base64 string.
 */
std::vector<uint8_t> base64_decode_vector(const char *ptr, size_t length);

/**
 * @brief Decodes the specified base64 encoded string to a byte vector.
 *
 * @param str The base64 string to be decoded.
 * @return std::vector<uint8_t> A vector containing the data decoded from the
 * base64 string.
 */
inline std::vector<uint8_t> base64_decode_vector(std::string_view str) {
  return base64_decode_vector(str.data(), str.size());
}

/**
 * @brief Decodes the specified base64 string.
 *
 * @param ptr A pointer to the base64 string to be decoded.
 * @param length The length, in characters, of the base64 string to be decoded.
 * @return std::string The decoded string.
 */
std::string base64_decode_string(const char *ptr, size_t length);

/**
 * @brief Decodes the specified base64 string.
 *
 * @param str The base64 string to be decoded.
 * @return std::string The decoded string.
 */
inline std::string base64_decode_string(std::string_view str) {
  return base64_decode_string(str.data(), str.size());
}
