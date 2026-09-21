#include "draft_schema_view_test_base.hpp"
#include "draft_schema_valid.h"
#include <format>
#include <string>

class DraftSchemaViewSimpleSchemaTests : public DraftSchemaViewSchemaTestBase {

public:
  DraftSchemaViewSimpleSchemaTests()
      : DraftSchemaViewSchemaTestBase(R"(
    {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "$id": "https://example.com/schemas/simple_schema.json",
        "title": "Simple Schema",
        "description": "Simple schema for testing root basic type validation.",
        "type": "object",
        "properties": {
            "integer_value": {
                "type": "integer",
                "description": "Integer value, which we'll use as a primary key in our table."
            },
            "null_value": {
                "type": "null",
                "description": "Null value example"
            },
            "boolean_value": {
              "type": "boolean",
              "description": "A boolean value; not to be confused with an integer."
            },
            "double_value": {
                "type": "number",
                "description": "A double-precision floating-point value for test."                    
            },
            "text_value": {
                "type": "string",
                "description": "A string value for test."
            }
        },
        "required": [
            "integer_value",
            "null_value",
            "boolean_value",
            "double_value",
            "text_value"
        ],
        "additionalProperties": false
    }
  )") {}

protected:
  void SetUp() override {
    DraftSchemaViewSchemaTestBase::SetUp();

    const std::string create_table_qry = R"(
            CREATE TABLE simple_schema_store (
                integer_value INTEGER PRIMARY KEY, 
                null_value TEXT,
                boolean_value INTEGER, 
                double_value REAL,
                text_value TEXT NOT NULL
            )
        )";

    int rc = sqlite3_exec(Database(), create_table_qry.c_str(), nullptr,
                          nullptr, nullptr);
    ASSERT_EQ(rc, SQLITE_OK) << "Failed to create simple_schema_store table: "
                             << sqlite3_errmsg(Database());
  }

  void create_simple_schema_view(const std::string_view vtable_name,
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
            'simple_schema_store',
            '{}',
            'integer_value'
        );
    )",
                    vtable_name, type_str, schema_uri);

    int rc = sqlite3_exec(Database(), create_table.c_str(), nullptr, nullptr,
                          nullptr);
    EXPECT_EQ(rc, SQLITE_OK)
        << "Error creating " << type_str << " vtable, " << vtable_name
        << ", over simple_schema_store, with validation from " << schema_uri
        << ": " << sqlite3_errmsg(Database());
  }
};

TEST_F(DraftSchemaViewSimpleSchemaTests, draft_schema_valid_simple_valid_case) {

  const std::string validate_query = std::format(R"(
        SELECT draft_schema_valid('{}',
            'integer_value', 1,
            'null_value', NULL,
            'boolean_value', false,
            'double_value', 1.234,
            'text_value', 'Some Text'
        );
    )",
                                                 schema_uri);

  sqlite3_stmt *stmt = nullptr;
  auto rc = sqlite3_prepare_v2(Database(), validate_query.c_str(), -1, &stmt,
                               nullptr);
  ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare draft_schema_valid call: "
                           << sqlite3_errmsg(Database());

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW)
      << "Expected a single row result from draft_schema_valid query.";

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 1) << "Expected a single column result from "
                        "draft_schema_valid query, received "
                     << std::to_string(colc) << " columns.";

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0))
      << "Expected result of draft_schama_valid query to SQLITE_INTEGER, "
         "received "
      << std::to_string(sqlite3_column_type(stmt, 0));
  auto res = sqlite3_column_int(stmt, 0);
  ASSERT_EQ(1, res) << "Expected result to be true; received "
                    << (res == 0 ? "false" : std::to_string(res));

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE)
      << "Expacted only one row from draft_schema_valid query.";
}

TEST_F(DraftSchemaViewSimpleSchemaTests,
       draft_schema_valid_simple_invalid_case) {

  const std::string validate_query = std::format(R"(
        SELECT draft_schema_valid('{}',
            'integer_value', 'Not an integer',
            'null_value', 42,
            'boolean_value', true,
            'double_value', '1.234',
            'text_value', false
        );
    )",
                                                 schema_uri);

  sqlite3_stmt *stmt = nullptr;
  auto rc = sqlite3_prepare_v2(Database(), validate_query.c_str(), -1, &stmt,
                               nullptr);
  ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare draft_schema_valid call: "
                           << sqlite3_errmsg(Database());

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW)
      << "Expected a single row result from draft_schema_valid query.";

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 1) << "Expected a single column result from "
                        "draft_schema_valid query, received "
                     << std::to_string(colc) << " columns.";

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0))
      << "Expected result of draft_schama_valid query to SQLITE_INTEGER, "
         "received "
      << std::to_string(sqlite3_column_type(stmt, 0));
  auto res = sqlite3_column_int(stmt, 0);
  ASSERT_EQ(0, res) << "Expected result to be false; received "
                    << (res == 1 ? "true" : std::to_string(res));

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE)
      << "Expacted only one row from draft_schema_valid query.";
}

TEST_F(DraftSchemaViewSimpleSchemaTests, filter_simple_valid_case) {

  ASSERT_NO_FATAL_FAILURE(create_simple_schema_view(
      "simple_schema_view", DRAFT_SCHEMA_VIEW_VTAB_FILTER));

  const std::string insert_qry = R"(
        INSERT INTO simple_schema_view (integer_value, null_value, boolean_value, double_value, text_value)
        VALUES(1, NULL, true, 2.34, 'Some Text');
    )";

  char *pzErr = nullptr;
  auto rc =
      sqlite3_exec(Database(), insert_qry.c_str(), nullptr, nullptr, &pzErr);
  ASSERT_EQ(rc, SQLITE_OK) << "Failed to insert into simple_schema_view: "
                           << pzErr;

  const std::string select_qry = R"(
        SELECT integer_value, null_value, boolean_value, double_value, text_value
        FROM simple_schema_view;
    )";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(Database(), select_qry.c_str(), -1, &stmt, nullptr);

  StatementFinalizer defer_select_finalize(stmt);
  ASSERT_EQ(rc, SQLITE_OK)
      << "Failed to prepare SELECT statemnt for simple_schema_view: "
      << sqlite3_errmsg(Database());

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW) << "Expected one row from simple_schema_view.";

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 5)
      << "Expected 5 columns from simple_schema_view query; received "
      << std::to_string(colc);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0))
      << "Expected column 0 of type SQLITE_INTEGER (0); received "
      << std::to_string(sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0))
      << "Expected value of column 0 to be 1; received "
      << std::to_string(sqlite3_column_int(stmt, 0));

  ASSERT_EQ(SQLITE_NULL, sqlite3_column_type(stmt, 1))
      << "Expected column 1 of type SQLITE_NULL (5); received "
      << std::to_string(sqlite3_column_type(stmt, 1));

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 2))
      << "Expected column 2 of type SQLITE_INTEGER(1); received "
      << std::to_string(sqlite3_column_type(stmt, 2));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 2))
      << "Expected value of column 2 to be true; received "
      << std::to_string(sqlite3_column_int(stmt, 2));

  ASSERT_EQ(SQLITE_FLOAT, sqlite3_column_type(stmt, 3))
      << "Expected column 3 of type SQLITE_FLOAT (2); received "
      << std::to_string(sqlite3_column_type(stmt, 3));
  ASSERT_DOUBLE_EQ(2.34, sqlite3_column_double(stmt, 3))
      << "Expected value of column 3 to be 2.34; received "
      << std::to_string(sqlite3_column_double(stmt, 3));

  ASSERT_EQ(SQLITE3_TEXT, sqlite3_column_type(stmt, 4))
      << "Expected column 4 to be of type SQLITE_TEXT(3); received "
      << std::to_string(sqlite3_column_type(stmt, 4));
  auto txt =
      read_string(sqlite3_column_text(stmt, 4), sqlite3_column_bytes(stmt, 4));
  ASSERT_EQ(txt, "Some Text")
      << "Expected value of column 4 to be 'Some Text', received '" << txt
      << "'";

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE) << "Expected only one row.";
}

TEST_F(DraftSchemaViewSimpleSchemaTests, shadow_simple_valid_case) {

  // Create shadow view over empty table (nothing goes into shadow table yet)
  ASSERT_NO_FATAL_FAILURE(create_simple_schema_view(
      "simple_schema_view", DRAFT_SCHEMA_VIEW_VTAB_SHADOW));

  // Insert valid item (passes validation) - the exercises the triggers and
  // populates the shadow as well as the target table
  const std::string insert_qry = R"(
        INSERT INTO simple_schema_view (integer_value, null_value, boolean_value, double_value, text_value)
        VALUES(1, NULL, TRUE, 2.34, 'Some Text');
    )";
  char *pzErr = nullptr;
  auto rc =
      sqlite3_exec(Database(), insert_qry.c_str(), nullptr, nullptr, &pzErr);
  ASSERT_EQ(rc, SQLITE_OK) << "Error inserting into simple_schema_view: "
                           << pzErr;

  // Select the item we inserted
  const std::string select_qry = R"(
        SELECT integer_value, null_value, boolean_value, double_value, text_value
        FROM simple_schema_view;
    )";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(Database(), select_qry.c_str(), -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK) << "Failed to prepare select statement: "
                           << sqlite3_errmsg(Database());

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW)
      << "Expected one row: "
      << (rc == SQLITE_DONE ? "none received" : sqlite3_errmsg(Database()));

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 5) << "Expected 5 columns in simple_schema_view, got "
                     << std::to_string(colc);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0))
      << "Expected column 0 to be an SQLITE_INTEGER (1), but received "
      << std::to_string(sqlite3_column_type(stmt, 0));

  ASSERT_EQ(SQLITE_NULL, sqlite3_column_type(stmt, 1))
      << "Expected columm 1 to be SQLITE_NULL (5), but received "
      << std::to_string(sqlite3_column_type(stmt, 1));

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 2))
      << "Expected column 2 to be SQLITE_INTEGER (1 - acting as a boolean), "
         "but received "
      << sqlite3_column_type(stmt, 2);
  ASSERT_EQ(1, sqlite3_column_int(stmt, 2))
      << "Expected value of column 2 to be TRUE (1), but received "
      << std::to_string(sqlite3_column_int(stmt, 2));

  ASSERT_EQ(SQLITE_FLOAT, sqlite3_column_type(stmt, 3))
      << "Expected column 3 to be SQLITE_FLOAT (2), but received "
      << std::to_string(sqlite3_column_type(stmt, 3));
  ASSERT_DOUBLE_EQ(2.34, sqlite3_column_double(stmt, 3))
      << "Expected value of column 3 to be 2.34, but received "
      << std::to_string(sqlite3_column_double(stmt, 3));

  ASSERT_EQ(SQLITE3_TEXT, sqlite3_column_type(stmt, 4))
      << "Expected column 4 to be SQLITE_TEXT (3), but received "
      << std::to_string(sqlite3_column_type(stmt, 4));
  auto txt =
      read_string(sqlite3_column_text(stmt, 4), sqlite3_column_bytes(stmt, 4));
  ASSERT_EQ(txt, "Some Text")
      << "Expected value of column 4 to be 'Some Text', but received '" << txt
      << "'";

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE) << "Expected only one row";

  // Create a second view now that we have data to see it pop into the shadow
  ASSERT_NO_FATAL_FAILURE(create_simple_schema_view(
      "simple_schema_view_2", DRAFT_SCHEMA_VIEW_VTAB_SHADOW));

  const std::string select_qry_2 = R"(
        SELECT integer_value, null_value, boolean_value, double_value, text_value
        FROM simple_schema_view_2;
    )";

  stmt = nullptr;
  rc = sqlite3_prepare_v2(Database(), select_qry_2.c_str(), -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK)
      << "Failed to prepare SELECT query for simple_schema_view_2: "
      << sqlite3_errmsg(Database());

  StatementFinalizer defer_select_stmt_2_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW)
      << "Expected one row: "
      << (rc == SQLITE_DONE ? "none received" : sqlite3_errmsg(Database()));

  colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 5) << "Expected 5 columns in simple_schema_view, got "
                     << std::to_string(colc);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0))
      << "Expected column 0 to be an SQLITE_INTEGER (1), but received "
      << std::to_string(sqlite3_column_type(stmt, 0));

  ASSERT_EQ(SQLITE_NULL, sqlite3_column_type(stmt, 1))
      << "Expected columm 1 to be SQLITE_NULL (5), but received "
      << std::to_string(sqlite3_column_type(stmt, 1));

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 2))
      << "Expected column 2 to be SQLITE_INTEGER (1 - acting as a boolean), "
         "but received "
      << sqlite3_column_type(stmt, 2);
  ASSERT_EQ(1, sqlite3_column_int(stmt, 2))
      << "Expected value of column 2 to be TRUE (1), but received "
      << std::to_string(sqlite3_column_int(stmt, 2));

  ASSERT_EQ(SQLITE_FLOAT, sqlite3_column_type(stmt, 3))
      << "Expected column 3 to be SQLITE_FLOAT (2), but received "
      << std::to_string(sqlite3_column_type(stmt, 3));
  ASSERT_DOUBLE_EQ(2.34, sqlite3_column_double(stmt, 3))
      << "Expected value of column 3 to be 2.34, but received "
      << std::to_string(sqlite3_column_double(stmt, 3));

  ASSERT_EQ(SQLITE3_TEXT, sqlite3_column_type(stmt, 4))
      << "Expected column 4 to be SQLITE_TEXT (3), but received "
      << std::to_string(sqlite3_column_type(stmt, 4));
  txt =
      read_string(sqlite3_column_text(stmt, 4), sqlite3_column_bytes(stmt, 4));
  ASSERT_EQ(txt, "Some Text")
      << "Expected value of column 4 to be 'Some Text', but received '" << txt
      << "'";

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE) << "Expected only one row";
}

TEST_F(DraftSchemaViewSimpleSchemaTests, teardown_vtable) {
  ASSERT_NO_FATAL_FAILURE(create_simple_schema_view(
      "simple_schema_view", DRAFT_SCHEMA_VIEW_VTAB_FILTER));

  const std::string drop_vtable_sql = "DROP TABLE simple_schema_view;";

  char *errmsg;
  auto rc = sqlite3_exec(Database(), drop_vtable_sql.c_str(), nullptr, nullptr,
                         &errmsg);

  ASSERT_EQ(rc, SQLITE_OK) << "Error dropping vtable: " << errmsg;
}

TEST_F(DraftSchemaViewSimpleSchemaTests, draft_schema_valid_error_cases) {
  // 1. Argument count checks
  ExpectSqlError(
      "SELECT draft_schema_valid('https://example.com/schemas/simple_schema.json');",
      "Usage: draft_schema_valid"
  );
  ExpectSqlError(
      "SELECT draft_schema_valid('https://example.com/schemas/simple_schema.json', 'integer_value');",
      "Usage: draft_schema_valid"
  );

  // 2. Null/Empty URI checks
  ExpectSqlError(
      "SELECT draft_schema_valid(NULL, 'integer_value', 1);",
      "schema_uri cannot be null or empty."
  );
  ExpectSqlError(
      "SELECT draft_schema_valid('', 'integer_value', 1);",
      "schema_uri cannot be null or empty."
  );

  // 3. Unregistered schema URI check
  ExpectSqlError(
      "SELECT draft_schema_valid('https://example.com/unregistered.json', 'integer_value', 1);",
      "The specified schema is not registered."
  );
}

TEST_F(DraftSchemaViewSimpleSchemaTests, null_state_invariant_test) {
  // Register a temporary SQL function with null user data to trigger the nullptr state check
  int rc = sqlite3_create_function_v2(
      Database(), "draft_schema_valid_null_state", -1,
      SQLITE_UTF8, nullptr, draft_schema_valid, nullptr, nullptr, nullptr
  );
  ASSERT_EQ(rc, SQLITE_OK);

  ExpectSqlError(
      "SELECT draft_schema_valid_null_state('https://example.com/schemas/simple_schema.json', 'integer_value', 1);",
      "Failed to obtain draft schema viewer state."
  );
}