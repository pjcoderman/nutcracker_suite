
#include <gtest/gtest.h>

#include "draft_schema_view.h"
#include "dsv_common.h"
#include "dsv_module.h"
#include "dsv_registry.h"
#include "dsv_sql_helpers.h"
#include "dsv_string_helpers.h"
#include "dsv_vtable_triggers.h"

#include <iostream>
#include <vector>

#define VIEW_REGISTRY_TABLE_NAME "test_draft_schema_view_registry"
#define SCHEMA_REGISTRY_TABLE_NAME "test_draft_schema_view_schema_registry"

static sqlite3 *create_test_memory_database();
static void close_database(sqlite3 *db);
static void ensure_view_registry_table_info(sqlite3 *db);
static void ensure_schema_registry_table_info(sqlite3 *db);
static void ensure_table_info(sqlite3 *db, const char *pTableName,
                              const std::vector<ColumnDef> &columns);
static void
create_simple_schema_table(sqlite3 *db,
                           const char *table_name = "simple_schema_store");
static void create_simple_schema_view(
    sqlite3 *db, const char *vtable_name = "simple_schema_view",
    const DraftSchemaViewVTabType type = DRAFT_SCHEMA_VIEW_VTAB_SHADOW,
    const char *table_name = "simple_schema_store",
    const char *schema_uri = "https://example.com/schemas/simple_schema.json");

static const char *pSimpleSchema = R"(
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
)";

TEST(draft_schema_view_test,
     draft_schema_view_init_creates_view_registry_table) {
  auto db = create_test_memory_database();
  if (db == nullptr) {
    return;
  }

  int rc = draft_schema_view_init(db, VIEW_REGISTRY_TABLE_NAME,
                                  SCHEMA_REGISTRY_TABLE_NAME);
  EXPECT_EQ(rc, SQLITE_OK);

  ensure_schema_registry_table_info(db);
  close_database(db);
}

TEST(draft_schema_view_test,
     draft_schema_view_init_creates_schema_registry_table) {
  auto db = create_test_memory_database();
  if (db == nullptr) {
    return;
  }

  int rc = draft_schema_view_init(db, VIEW_REGISTRY_TABLE_NAME,
                                  SCHEMA_REGISTRY_TABLE_NAME);
  EXPECT_EQ(rc, SQLITE_OK);

  ensure_schema_registry_table_info(db);
  close_database(db);
}

TEST(draft_schema_view_test, draft_schema_valid_simple_valid_case) {
  auto db = create_test_memory_database();
  if (db == nullptr) {
    return;
  }

  int rc = draft_schema_view_init(db, VIEW_REGISTRY_TABLE_NAME,
                                  SCHEMA_REGISTRY_TABLE_NAME);
  ASSERT_EQ(rc, SQLITE_OK);

  nlohmann::json simple_schema = nlohmann::json::parse(pSimpleSchema);

  rc = draft_schema_view_insert_into_schema_registry(
      db, SCHEMA_REGISTRY_TABLE_NAME,
      "https://example.com/schemas/simple_schema.json", simple_schema);
  ASSERT_EQ(rc, SQLITE_OK);

  const char *validate_query = R"(
        SELECT draft_schema_valid('https://example.com/schemas/simple_schema.json',
            'integer_value', 1,
            'null_value', NULL,
            'boolean_value', false,
            'double_value', 1.234,
            'text_value', 'Some Text'
        );
    )";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(db, validate_query, -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW);

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 1);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0));

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE);

  close_database(db);
}

TEST(draft_schema_view_test, draft_schema_valid_simple_invalid_case) {
  auto db = create_test_memory_database();
  if (db == nullptr) {
    return;
  }

  int rc = draft_schema_view_init(db, VIEW_REGISTRY_TABLE_NAME,
                                  SCHEMA_REGISTRY_TABLE_NAME);
  ASSERT_EQ(rc, SQLITE_OK);

  nlohmann::json simple_schema = nlohmann::json::parse(pSimpleSchema);

  rc = draft_schema_view_insert_into_schema_registry(
      db, SCHEMA_REGISTRY_TABLE_NAME,
      "https://example.com/schemas/simple_schema.json", simple_schema);
  ASSERT_EQ(rc, SQLITE_OK);

  const char *validate_query = R"(
        SELECT draft_schema_valid('https://example.com/schemas/simple_schema.json',
            'integer_value', 'Not an integer',
            'null_value', 42,
            'boolean_value', true,
            'double_value', '1.234',
            'text_value', false
        );
    )";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(db, validate_query, -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW);

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 1);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(0, sqlite3_column_int(stmt, 0));

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE);

  close_database(db);
}

TEST(draft_schema_view_test, draft_schema_view_filter_simple_valid_case) {
  auto db = create_test_memory_database();
  if (db == nullptr) {
    return;
  }

  int rc = draft_schema_view_init(db, VIEW_REGISTRY_TABLE_NAME,
                                  SCHEMA_REGISTRY_TABLE_NAME);
  ASSERT_EQ(rc, SQLITE_OK);

  nlohmann::json simple_schema = nlohmann::json::parse(pSimpleSchema);

  rc = draft_schema_view_insert_into_schema_registry(
      db, SCHEMA_REGISTRY_TABLE_NAME,
      "https://example.com/schemas/simple_schema.json", simple_schema);
  ASSERT_EQ(rc, SQLITE_OK);

  create_simple_schema_table(db, "simple_schema_store");
  create_simple_schema_view(
      db, "simple_schema_view", DRAFT_SCHEMA_VIEW_VTAB_FILTER,
      "simple_schema_store", "https://example.com/schemas/simple_schema.json");

  const char *insert_qry = R"(
        INSERT INTO simple_schema_view (integer_value, null_value, boolean_value, double_value, text_value)
        VALUES(1, NULL, true, 2.34, 'Some Text');
    )";

  char *pzErr = nullptr;
  rc = sqlite3_exec(db, insert_qry, nullptr, nullptr, &pzErr);
  ASSERT_EQ(rc, SQLITE_OK);

  const char *select_qry = R"(
        SELECT integer_value, null_value, boolean_value, double_value, text_value
        FROM simple_schema_view;
    )";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(db, select_qry, -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW);

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 5);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0));

  ASSERT_EQ(SQLITE_NULL, sqlite3_column_type(stmt, 1));

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 2));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 2));

  ASSERT_EQ(SQLITE_FLOAT, sqlite3_column_type(stmt, 3));
  ASSERT_DOUBLE_EQ(2.34, sqlite3_column_double(stmt, 3));

  ASSERT_EQ(SQLITE3_TEXT, sqlite3_column_type(stmt, 4));
  auto txt =
      read_string(sqlite3_column_text(stmt, 4), sqlite3_column_bytes(stmt, 4));
  ASSERT_EQ(txt, "Some Text");

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE);

  close_database(db);
}

TEST(draft_schema_view_test, draft_schema_view_shadow_simple_valid_case) {
  auto db = create_test_memory_database();
  if (db == nullptr) {
    return;
  }

  int rc = draft_schema_view_init(db, VIEW_REGISTRY_TABLE_NAME,
                                  SCHEMA_REGISTRY_TABLE_NAME);
  ASSERT_EQ(rc, SQLITE_OK);

  nlohmann::json simple_schema = nlohmann::json::parse(pSimpleSchema);

  rc = draft_schema_view_insert_into_schema_registry(
      db, SCHEMA_REGISTRY_TABLE_NAME,
      "https://example.com/schemas/simple_schema.json", simple_schema);
  ASSERT_EQ(rc, SQLITE_OK);

  create_simple_schema_table(db, "simple_schema_store");
  create_simple_schema_view(
      db, "simple_schema_view", DRAFT_SCHEMA_VIEW_VTAB_SHADOW,
      "simple_schema_store", "https://example.com/schemas/simple_schema.json");

  const char *insert_qry = R"(
        INSERT INTO simple_schema_view (integer_value, null_value, boolean_value, double_value, text_value)
        VALUES(1, NULL, TRUE, 2.34, 'Some Text');
    )";

  char *pzErr = nullptr;
  rc = sqlite3_exec(db, insert_qry, nullptr, nullptr, &pzErr);
  ASSERT_EQ(rc, SQLITE_OK);

  const char *select_qry = R"(
        SELECT integer_value, null_value, boolean_value, double_value, text_value
        FROM simple_schema_view;
    )";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(db, select_qry, -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);

  StatementFinalizer defer_select_finalize(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW);

  auto colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 5);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0));

  ASSERT_EQ(SQLITE_NULL, sqlite3_column_type(stmt, 1));

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 2));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 2));

  ASSERT_EQ(SQLITE_FLOAT, sqlite3_column_type(stmt, 3));
  ASSERT_DOUBLE_EQ(2.34, sqlite3_column_double(stmt, 3));

  ASSERT_EQ(SQLITE3_TEXT, sqlite3_column_type(stmt, 4));
  auto txt =
      read_string(sqlite3_column_text(stmt, 4), sqlite3_column_bytes(stmt, 4));
  ASSERT_EQ(txt, "Some Text");

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE);

  create_simple_schema_view(
      db, "simple_schema_view2", DRAFT_SCHEMA_VIEW_VTAB_SHADOW,
      "simple_schema_store", "https://example.com/schemas/simple_schema.json");

  const char *select_qry2 = R"(
        SELECT integer_value, null_value, boolean_value, double_value, text_value
        FROM simple_schema_view2;
    )";

  stmt = nullptr;
  rc = sqlite3_prepare_v2(db, select_qry2, -1, &stmt, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);

  StatementFinalizer defer_select_finalize2(stmt);

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_ROW);

  colc = sqlite3_column_count(stmt);
  ASSERT_EQ(colc, 5);

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 0));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 0));

  ASSERT_EQ(SQLITE_NULL, sqlite3_column_type(stmt, 1));

  ASSERT_EQ(SQLITE_INTEGER, sqlite3_column_type(stmt, 2));
  ASSERT_EQ(1, sqlite3_column_int(stmt, 2));

  ASSERT_EQ(SQLITE_FLOAT, sqlite3_column_type(stmt, 3));
  ASSERT_DOUBLE_EQ(2.34, sqlite3_column_double(stmt, 3));

  ASSERT_EQ(SQLITE3_TEXT, sqlite3_column_type(stmt, 4));
  txt =
      read_string(sqlite3_column_text(stmt, 4), sqlite3_column_bytes(stmt, 4));
  ASSERT_EQ(txt, "Some Text");

  rc = sqlite3_step(stmt);
  ASSERT_EQ(rc, SQLITE_DONE);

  close_database(db);
}

static sqlite3 *create_test_memory_database() {
  sqlite3 *db = nullptr;
  int rc = sqlite3_open_v2(
      ":memory:", // Magic filename for in-memory DB
      &db,        // Output: SQLite db handle
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, // Required flags
      nullptr // Name of VFS module (nullptr uses default)
  );
  EXPECT_EQ(rc, SQLITE_OK);
  if (rc != SQLITE_OK) {
    auto msg = sqlite3_errmsg(db);
    std::cerr << "Error opening database: " << msg << std::endl;
  }
  return db;
}

static void close_database(sqlite3 *db) {
  EXPECT_NE(db, nullptr);
  if (db == nullptr) {
    return;
  }

  auto rc = sqlite3_close_v2(db);
  if (rc != SQLITE_OK) {
    auto msg = sqlite3_errmsg(db);
    std::cerr << "Error closing database: " << msg << std::endl;
  }

  EXPECT_EQ(rc, SQLITE_OK);
}

static void ensure_view_registry_table_info(sqlite3 *db) {
  std::vector<ColumnDef> expected_columns;
  expected_columns.push_back(
      ColumnDef(0, "vtable_name", "TEXT", false, "", true));
  expected_columns.push_back(
      ColumnDef(1, "table_name", "TEXT", true, "", false));
  expected_columns.push_back(
      ColumnDef(2, "schema_uri", "TEXT", true, "", false));
  expected_columns.push_back(ColumnDef(3, "key_cols", "TEXT", true, "", false));
  ensure_table_info(db, VIEW_REGISTRY_TABLE_NAME, expected_columns);
}

static void ensure_schema_registry_table_info(sqlite3 *db) {
  std::vector<ColumnDef> expected_columns;
  expected_columns.push_back(
      ColumnDef(0, "schema_uri", "TEXT", true, "", true));
  expected_columns.push_back(
      ColumnDef(1, "schema_json", "TEXT", true, "", false));
  ensure_table_info(db, SCHEMA_REGISTRY_TABLE_NAME, expected_columns);
}

static void ensure_table_info(sqlite3 *db, const char *pTableName,
                              const std::vector<ColumnDef> &columns) {

  EXPECT_NE(db, nullptr);
  EXPECT_NE(pTableName, nullptr);
  if (db == nullptr || pTableName == nullptr) {
    return;
  }

  std::cout << "Ensuring table: " << pTableName << std::endl;

  char *pragma_table_info_qry =
      sqlite3_mprintf("PRAGMA table_info(%Q)", pTableName);
  EXPECT_NE(pragma_table_info_qry, nullptr);

  sqlite3_stmt *pragma_table_info_stmt = nullptr;
  auto rc = sqlite3_prepare_v2(db, pragma_table_info_qry, -1,
                               &pragma_table_info_stmt, nullptr);
  sqlite3_free(pragma_table_info_qry);
  EXPECT_EQ(rc, SQLITE_OK);

  auto column_idx = 0;

  while (rc != SQLITE_DONE) {
    rc = sqlite3_step(pragma_table_info_stmt);
    if (column_idx < columns.size()) {
      EXPECT_EQ(rc, SQLITE_ROW);
    } else {
      EXPECT_EQ(rc, SQLITE_DONE);
    }
    if (rc != SQLITE_ROW) {
      break;
    }
    auto &col = columns.at(column_idx);

    auto num_cols = sqlite3_column_count(pragma_table_info_stmt);
    EXPECT_GE(num_cols, 6);

    auto col_typ = sqlite3_column_type(pragma_table_info_stmt, 0);
    EXPECT_EQ(col_typ, SQLITE_INTEGER);
    auto cid = sqlite3_column_int(pragma_table_info_stmt, 0);
    std::cout << "cid: " << std::to_string(cid) << std::endl;
    EXPECT_EQ(cid, column_idx);

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 1);
    EXPECT_EQ(col_typ, SQLITE_TEXT);
    auto column_name_len = sqlite3_column_bytes(pragma_table_info_stmt, 1);
    EXPECT_GT(column_name_len, 0);
    auto pColumnName = reinterpret_cast<const char *>(
        sqlite3_column_text(pragma_table_info_stmt, 1));
    EXPECT_NE(pColumnName, nullptr);
    auto column_name =
        std::string_view(pColumnName, pColumnName + column_name_len);
    std::cout << "name: " << column_name << std::endl;
    EXPECT_EQ(column_name, col.get_name());

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 2);
    EXPECT_EQ(col_typ, SQLITE_TEXT);
    auto column_type_len = sqlite3_column_bytes(pragma_table_info_stmt, 2);
    EXPECT_GT(column_type_len, 0);
    auto pColumnType = reinterpret_cast<const char *>(
        sqlite3_column_text(pragma_table_info_stmt, 2));
    EXPECT_NE(pColumnType, nullptr);
    auto column_type =
        std::string_view(pColumnType, pColumnType + column_type_len);
    std::cout << "type: " << column_type << std::endl;
    EXPECT_EQ(column_type, col.get_type());

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 3);
    EXPECT_EQ(col_typ, SQLITE_INTEGER);
    auto notnull = sqlite3_column_int(pragma_table_info_stmt, 3);
    std::cout << "notnull: " << (notnull ? "true" : "false") << std::endl;
    EXPECT_EQ(notnull, col.is_not_null() ? 1 : 0);

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 4);
    auto col_dflt_val = col.get_dflt_value();
    if (col_dflt_val.size() != 0) {
      EXPECT_EQ(col_typ, SQLITE_TEXT);
    } else {
      EXPECT_TRUE(col_typ == SQLITE_TEXT || col_typ == SQLITE_NULL);
    }

    auto default_value_len = sqlite3_column_bytes(pragma_table_info_stmt, 4);
    auto pDefaultValue = reinterpret_cast<const char *>(
        sqlite3_column_text(pragma_table_info_stmt, 4));
    std::string_view default_value;
    if (pDefaultValue != nullptr && default_value_len > 0) {
      default_value =
          std::string_view(pDefaultValue, pDefaultValue + default_value_len);
      std::cout << "dflt_value: " << default_value << std::endl;
    } else {
      std::cout << "dflt_value: "
                << std::to_string((size_t)(void *)pDefaultValue)
                << " (length=" << std::to_string(default_value_len) << ")"
                << std::endl;
    }
    EXPECT_EQ(default_value, col_dflt_val);

    auto pk = sqlite3_column_int(pragma_table_info_stmt, 5);
    std::cout << "pk: " << (pk ? "true" : "false") << std::endl;
    EXPECT_EQ(pk, col.is_pk() ? 1 : 0);

    column_idx++;
  }

  EXPECT_EQ(column_idx, columns.size());

  std::cout << "finalizing pragma for view registry table" << std::endl;
  rc = sqlite3_finalize(pragma_table_info_stmt);
  EXPECT_EQ(rc, SQLITE_OK);
}

static void create_simple_schema_view(sqlite3 *db, const char *vtable_name,
                                      const DraftSchemaViewVTabType type,
                                      const char *table_name,
                                      const char *schema_uri) {
  EXPECT_NE(db, nullptr);
  EXPECT_NE(vtable_name, nullptr);
  EXPECT_NE(schema_uri, nullptr);
  if (vtable_name == nullptr || db == nullptr || schema_uri == nullptr) {
    return;
  }

  const char *type_str;
  if (type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW) {
    type_str = "SHADOW";
  } else if (type == DRAFT_SCHEMA_VIEW_VTAB_FILTER) {
    type_str = "FILTER";
  } else {
    FAIL();
    return;
  }

  const char *pCreateTableFmt = R"(
        CREATE VIRTUAL TABLE %Q USING draft_schema_view(
            %Q,
            %Q,
            %Q,
            'integer_value'
        );
    )";

  char *pCreateTable = sqlite3_mprintf(pCreateTableFmt, vtable_name, type_str,
                                       table_name, schema_uri);
  EXPECT_NE(pCreateTable, nullptr);
  if (pCreateTable == nullptr) {
    FAIL();
    return;
  }

  sqlite3_stmt *stmt;
  int rc = sqlite3_exec(db, pCreateTable, nullptr, nullptr, nullptr);
  sqlite3_free(pCreateTable);
  EXPECT_EQ(rc, SQLITE_OK);
}

void create_simple_schema_table(sqlite3 *db, const char *table_name) {
  ASSERT_NE(db, nullptr);
  ASSERT_NE(table_name, nullptr);
  if (table_name == nullptr || db == nullptr) {
    return;
  }

  const std::string create_table_qry = std::format(R"(
        CREATE TABLE {} (
            integer_value INTEGER PRIMARY KEY, 
            null_value TEXT,
            boolean_value INTEGER, 
            double_value REAL,
            text_value TEXT NOT NULL
        )
    )",
                                                   table_name);

  int rc =
      sqlite3_exec(db, create_table_qry.c_str(), nullptr, nullptr, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);
}
