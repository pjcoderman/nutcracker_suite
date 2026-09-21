#include <gtest/gtest.h>

#include "base64.h"
#include "base64_encoding_tables.hpp"

class DraftSchemaViewBase64Tests : public ::testing::Test {};

TEST_F(DraftSchemaViewBase64Tests, base64_encode_hello_world_test) {
  const std::string hello_world_str = "Hello World";
  auto encoded_str = base64_encode(hello_world_str);
  ASSERT_EQ(encoded_str, "SGVsbG8gV29ybGQ=");
}

TEST_F(DraftSchemaViewBase64Tests, base64_encode_1_10_test) {
  const std::vector<uint8_t> one_through_ten = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto encoded_str = base64_encode(one_through_ten);
  ASSERT_EQ(encoded_str, "AQIDBAUGBwgJCg==");
}

TEST_F(DraftSchemaViewBase64Tests, base64_decode_string_hello_world_test) {
  const std::string encoded_str = "SGVsbG8gV29ybGQ=";
  auto hello_world_str = base64_decode_string(encoded_str);
  ASSERT_EQ(hello_world_str, "Hello World");
}

TEST_F(DraftSchemaViewBase64Tests, base64_decode_vector_1_10_test) {
  const std::string encoded_str = "AQIDBAUGBwgJCg==";
  auto one_through_ten = base64_decode_vector(encoded_str);
  const std::vector<uint8_t> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  ASSERT_EQ(one_through_ten, expected);
}

TEST(Base64InternalTest, MakeDecodingTableGeneratesValidMap) {
  // Calling it without 'constexpr' forces runtime execution,
  // satisfying gcov/llvm-cov instrumentation.
  const auto table = base64_encoding_tables::make_decoding_table();

  // Spot-check standard boundaries
  EXPECT_EQ(table['A'], 0);
  EXPECT_EQ(table['Z'], 25);
  EXPECT_EQ(table['a'], 26);
  EXPECT_EQ(table['z'], 51);
  EXPECT_EQ(table['0'], 52);
  EXPECT_EQ(table['9'], 61);
  EXPECT_EQ(table['+'], 62);
  EXPECT_EQ(table['/'], 63);

  // Spot-check invalid entries
  EXPECT_EQ(table['='], -1);
  EXPECT_EQ(table['\0'], -1);
  EXPECT_EQ(table[' '], -1);
  EXPECT_EQ(table[255], -1);
}

TEST_F(DraftSchemaViewBase64Tests,
       base64_decode_string_invalid_characters_throw) {
  // Disallowed punctuation / symbols
  EXPECT_THROW(base64_decode_string("SGVs!G8gV29ybGQ="), std::invalid_argument);
  EXPECT_THROW(base64_decode_string("SGVsbG8#V29ybGQ="), std::invalid_argument);

  // Whitespace / control characters
  EXPECT_THROW(base64_decode_string("SGVs bG8gV29ybGQ="),
               std::invalid_argument);
  EXPECT_THROW(base64_decode_string("SGVs\nG8gV29ybGQ="),
               std::invalid_argument);

  // Extended / high-byte ASCII (> 127)
  EXPECT_THROW(base64_decode_string("SGVs\x80G8gV29ybGQ="),
               std::invalid_argument);
  EXPECT_THROW(base64_decode_string("SGVs\xFFG8gV29ybGQ="),
               std::invalid_argument);
}

TEST_F(DraftSchemaViewBase64Tests,
       base64_decode_vector_invalid_characters_throw) {
  // Disallowed punctuation / symbols
  EXPECT_THROW(base64_decode_vector("AQIDB!UGBwgJCg=="), std::invalid_argument);
  EXPECT_THROW(base64_decode_vector("AQIDBAUGBwgJCg?="), std::invalid_argument);

  // Whitespace / control characters
  EXPECT_THROW(base64_decode_vector("AQID BAUGBwgJCg=="),
               std::invalid_argument);
}