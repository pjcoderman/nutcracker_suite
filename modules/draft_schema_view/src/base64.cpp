#include "base64.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Global lookup tables
constexpr char encoding_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr auto make_decoding_table() {
  std::array<int, 256> table{};
  table.fill(-1);
  for (int i = 0; i < 64; ++i) {
    table[static_cast<unsigned char>(encoding_table[i])] = i;
  }
  return table;
}
constexpr auto decoding_table = make_decoding_table();

// ============================================================================
// ENCODING CORE (Pointer + Length)
// ============================================================================
std::string base64_encode(const uint8_t *ptr, size_t length) {
  std::string out;
  out.reserve(((length + 2) / 3) * 4);

  int val = 0;
  int valb = -6;
  for (size_t i = 0; i < length; ++i) {
    val = (val << 8) + ptr[i];
    valb += 8;
    while (valb >= 0) {
      out.push_back(encoding_table[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    out.push_back(encoding_table[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while (out.size() % 4 != 0) {
    out.push_back('=');
  }
  return out;
}

// ============================================================================
// DECODING CORE (Pointer + Length)
// ============================================================================

std::vector<uint8_t> base64_decode_vector(const char *ptr, size_t length) {
  std::vector<uint8_t> out;
  out.reserve((length / 4) * 3);

  int val = 0;
  int valb = -8;
  for (size_t i = 0; i < length; ++i) {
    char c = ptr[i];
    if (c == '=')
      break; // Handle padding termination

    int sexagesimal = decoding_table[static_cast<unsigned char>(c)];
    if (sexagesimal == -1) {
      throw std::invalid_argument("Invalid character in Base64 string.");
    }

    val = (val << 6) + sexagesimal;
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

std::string base64_decode_string(const char *ptr, size_t length) {
  std::string out;
  out.reserve((length / 4) * 3);

  int val = 0;
  int valb = -8;
  for (size_t i = 0; i < length; ++i) {
    char c = ptr[i];
    if (c == '=')
      break;

    int sexagesimal = decoding_table[static_cast<unsigned char>(c)];
    if (sexagesimal == -1) {
      throw std::invalid_argument("Invalid character in Base64 string.");
    }

    val = (val << 6) + sexagesimal;
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}
