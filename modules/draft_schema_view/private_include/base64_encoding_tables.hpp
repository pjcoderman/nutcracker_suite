#pragma once
#include <array>

namespace base64_encoding_tables {

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

inline constexpr auto decoding_table = make_decoding_table();

}; // namespace base64_encoding_tables