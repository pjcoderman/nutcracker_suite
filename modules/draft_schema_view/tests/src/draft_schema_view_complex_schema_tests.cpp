#include "draft_schema_view_test_base.hpp"

class DraftSchemaViewComplexSchemaTests : public DraftSchemaViewSchemaTestBase {

public:
  DraftSchemaViewComplexSchemaTests()
      : DraftSchemaViewSchemaTestBase(R"(
        {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "$id": "https://example.com/schemas/complex_schema.json",
        "title": "Complex Schema",
        "type": "object",
        "properties": {
            "id": { "type": "integer" },
            "json_text": { 
                "type": ["object", "null"],
                "properties": {
                    "key": { "type": "string" }
                }
            },
            "bson_blob": { "type": ["object", "null"] },
            "base64_blob": { "type": ["string", "null"] },
            "array_data": { "type": ["array", "null"] }
        },
        "required": ["id"],
        "additionalProperties": false
    }
        )") {}

protected:
  void SetUp() override {
    DraftSchemaViewSchemaTestBase::SetUp();

    const std::string create_table_qry = R"(
        CREATE TABLE complex_schema_store (
            id INTEGER PRIMARY KEY,
            json_text TEXT,
            bson_blob BLOB,
            base64_blob BLOB,
            array_data TEXT
        )
    )";

    int rc = sqlite3_exec(Database(), create_table_qry.c_str(), nullptr,
                          nullptr, nullptr);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to create complex_schema_store table: "
                             << sqlite3_errmsg(Database());
  }

  void create_complex_schema_view(const std::string_view vtable_name,
                                  const DraftSchemaViewVTabType type) {
    std::string type_str;
    if (type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW) {
      type_str = "SHADOW";
    } else if (type == DRAFT_SCHEMA_VIEW_VTAB_FILTER) {
      type_str = "FILTER";
    } else {
      FAIL() << "Unknown draft_schema_view type " << std::to_string(type);
      return;
    }

    const std::string create_table =
        std::format(R"(
        CREATE VIRTUAL TABLE {} USING draft_schema_view(
            '{}',
            'complex_schema_store',
            '{}',
            'id'
        );
    )",
                    vtable_name, type_str, schema_uri);

    int rc = sqlite3_exec(Database(), create_table.c_str(), nullptr, nullptr,
                          nullptr);
    EXPECT_EQ(rc, SQLITE_OK)
        << "Error creating " << type_str << " vtable, " << vtable_name
        << ", over complex_schema_store, with validation from " << schema_uri
        << ": " << sqlite3_errmsg(Database());
  }
};

TEST_F(DraftSchemaViewComplexSchemaTests,
       draft_schema_valid_complex_valid_case) {
  int rc, res;
  sqlite3_stmt *stmt = nullptr;
  {
    // Test JSON in TEXT
    const std::string json_text_query = std::format(R"(
        SELECT draft_schema_valid('{}',
            'id', 1,
            'json_text', '{{"key": "value"}}'
        );
    )",
                                                    schema_uri);
    rc = sqlite3_prepare_v2(Database(), json_text_query.c_str(), -1, &stmt,
                            nullptr);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare draft_schema_valid call: "
                             << sqlite3_errmsg(Database());

    StatementFinalizer defer_stmt_finalize(stmt);

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected one row from draft_schema_valid call";

    res = sqlite3_column_int(stmt, 0);
    ASSERT_EQ(res, 1) << "Expected true from draft_schema_valid call; received "
                      << (res == 0 ? "false" : std::to_string(res));

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE)
        << "Expected only one row from draft_schema_valid call";
  }
  {
    // Test BSON in BLOB
    nlohmann::json bson_obj = {{"key", "bson"}};
    std::vector<uint8_t> bson_data = nlohmann::json::to_bson(bson_obj);

    const std::string bson_blob_query =
        "SELECT draft_schema_valid(?, 'id', 2, 'bson_blob', ?);";
    rc = sqlite3_prepare_v2(Database(), bson_blob_query.c_str(), -1, &stmt,
                            nullptr);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare draft_schema_valid call: "
                             << sqlite3_errmsg(Database());
    StatementFinalizer deferred_stmt_finalizer_2(stmt);

    rc = sqlite3_bind_text(stmt, 1, schema_uri.c_str(), -1, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to bind schema_uri query arg: "
                             << sqlite3_errmsg(Database());

    rc = sqlite3_bind_blob(stmt, 2, bson_data.data(), (int)bson_data.size(),
                           SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK)
        << "Failed to bind bson_blob query arg: " << sqlite3_errmsg(Database());

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected one row from draft_schema_valid call.";

    res = sqlite3_column_int(stmt, 0);
    ASSERT_EQ(res, 1) << "Expected true from draft_schema_valid call; received "
                      << (res == 0 ? "false" : std::to_string(res));

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE)
        << "Expected only one row from draft_schema_valid call";
  }
  {
    // Test Base64 (BLOB treated as string)
    const std::string base64_blob_query =
        "SELECT draft_schema_valid(?, 'id', 3, 'base64_blob', x'DEADBEEF');";
    rc = sqlite3_prepare_v2(Database(), base64_blob_query.c_str(), -1, &stmt,
                            nullptr);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare draft_schema_valid call: "
                             << sqlite3_errmsg(Database());

    StatementFinalizer deferred_stmt_finalizer_3(stmt);

    rc = sqlite3_bind_text(stmt, 1, schema_uri.c_str(), -1, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to bind schema_uri query arg: "
                             << sqlite3_errmsg(Database());

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected one row from draft_schema_valid call";

    res = sqlite3_column_int(stmt, 0);
    ASSERT_EQ(res, 1) << "Expected true from draft_schema_valid call; received "
                      << (res == 0 ? "false" : std::to_string(res));

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE)
        << "Expected only one row from draft_schema_valid call";
  }
}

TEST_F(DraftSchemaViewComplexSchemaTests, filter_complex_valid_case) {
  ASSERT_NO_FATAL_FAILURE(create_complex_schema_view(
      "complex_schema_view", DRAFT_SCHEMA_VIEW_VTAB_FILTER));

  // Insert JSON text
  const std::string insert_json =
      "INSERT INTO complex_schema_view (id, json_text) VALUES "
      "(1, '{\"key\": \"v1\"}');";
  int rc =
      sqlite3_exec(Database(), insert_json.c_str(), nullptr, nullptr, nullptr);
  ASSERT_EQ(rc, SQLITE_OK)
      << "Failed to insert json_text into complex_schema_view: "
      << sqlite3_errmsg(Database());

  sqlite3_stmt *stmt;
  {
    // Insert BSON blob
    nlohmann::json bson_obj = {{"key", "v2"}};
    std::vector<uint8_t> bson_data = nlohmann::json::to_bson(bson_obj);
    rc = sqlite3_prepare_v2(
        Database(),
        "INSERT INTO complex_schema_view (id, bson_blob) VALUES (2, ?);", -1,
        &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare bson_blob INSERT statement "
                                "for complex_schema_view "
                             << sqlite3_errmsg(Database());
    StatementFinalizer deferred_bson_insert_finalize(stmt);

    rc = sqlite3_bind_blob(stmt, 1, bson_data.data(), (int)bson_data.size(),
                           SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to bind bson_data for query arg: "
                             << sqlite3_errmsg(Database());

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE) << "Failed to insert to complex_schema_view: "
                               << sqlite3_errmsg(Database());
  }

  {
    rc = sqlite3_prepare_v2(
        Database(),
        "SELECT json_text, bson_blob FROM complex_schema_view ORDER BY id;", -1,
        &stmt, nullptr);

    // Verify retrieval
    ASSERT_EQ(rc, SQLITE_OK)
        << "Failed to prepare INSERT statement for complex_schema_view "
        << sqlite3_errmsg(Database());

    StatementFinalizer deferred_stmt_finalize_2(stmt);

    // Row 1: JSON text
    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected at least one row from complex_schema_view SELECT query";

    auto json_text_len = sqlite3_column_bytes(stmt, 0);
    auto json_text = sqlite3_column_text(stmt, 0);
    auto json_str = read_string(json_text, json_text_len);

    EXPECT_EQ(json_str, "{\"key\": \"v1\"}")
        << "json_text failed to round trip through insertion and selection";

    // Row 2: BSON blob
    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected two rows from complex_schema_view SELECT";

    auto col_type = sqlite3_column_type(stmt, 1);
    ASSERT_EQ(col_type, SQLITE_BLOB)
        << "bson_blob failed to round trip through insertion and selection";

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE)
        << "Expected exactly two rows from complex_schema_view SELECT";
  }
}

TEST_F(DraftSchemaViewComplexSchemaTests, shadow_complex_valid_case) {

  ASSERT_NO_FATAL_FAILURE(create_complex_schema_view(
      "complex_schema_view", DRAFT_SCHEMA_VIEW_VTAB_SHADOW));

  // Insert JSON text
  const std::string insert_json =
      "INSERT INTO complex_schema_view (id, json_text) "
      "VALUES (1, '{\"key\": \"shadow_v1\"}');";
  int rc =
      sqlite3_exec(Database(), insert_json.c_str(), nullptr, nullptr, nullptr);
  ASSERT_EQ(rc, SQLITE_OK) << "Failed to insert into complex_schema_view: "
                           << sqlite3_errmsg(Database());

  // Insert BSON blob
  nlohmann::json bson_obj = {{"key", "shadow_v2"}};
  std::vector<uint8_t> bson_data = nlohmann::json::to_bson(bson_obj);
  sqlite3_stmt *stmt;
  {
    rc = sqlite3_prepare_v2(
        Database(),
        "INSERT INTO complex_schema_view (id, bson_blob) VALUES (2, ?);", -1,
        &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK)
        << "Failed to prepare INSERT statement for complex_schema_view "
        << sqlite3_errmsg(Database());
    StatementFinalizer deferred_stmt_finalizer(stmt);

    rc = sqlite3_bind_blob(stmt, 1, bson_data.data(), (int)bson_data.size(),
                           SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK)
        << "Failed to bind bson_data for complex_schema_view insertion: "
        << sqlite3_errmsg(Database());

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE)
        << "Expected exactly one row from complex_schema_view SELECT";
  }
  {
    // Verify retrieval from SHADOW view
    rc = sqlite3_prepare_v2(
        Database(),
        "SELECT json_text, bson_blob FROM complex_schema_view ORDER BY id;", -1,
        &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK)
        << "Failed to prepare SELECT statement for complex_schema_view: "
        << sqlite3_errmsg(Database());

    StatementFinalizer deferred_stmt_finalize_2(stmt);

    // Row 1
    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected two rows from complex_schema_view SELECT";

    auto json_text_len = sqlite3_column_bytes(stmt, 0);
    auto json_text = sqlite3_column_text(stmt, 0);
    auto json_str = read_string(json_text, json_text_len);

    EXPECT_EQ(json_str, "{\"key\": \"shadow_v1\"}")
        << "Failed to round trip json_text through insertion and selection.";

    // Row 2
    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW)
        << "Expected two rows from complex_schema_view SELECT";

    auto col_type = sqlite3_column_type(stmt, 1);
    ASSERT_EQ(col_type, SQLITE_BLOB)
        << "Failed to round trip bson_blob through insertion and selection.";

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_DONE)
        << "Expected exactly two rows from complex_schema_view SELECT";
  }
}
