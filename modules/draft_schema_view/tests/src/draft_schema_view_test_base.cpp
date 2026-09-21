#include "draft_schema_view_test_base.hpp"

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

static sqlite3 *create_test_memory_database();

DraftSchemaViewTestBase::DraftSchemaViewTestBase(
    const std::string_view view_registry_table_name,
    const std::string_view schema_registry_table_name)
    : view_registry_table_name(view_registry_table_name),
      schema_registry_table_name(schema_registry_table_name) {}

void DraftSchemaViewTestBase::SetUp() {
  db.reset(create_test_memory_database());
  ASSERT_NE(db, nullptr) << "Database creation failed";

  int rc = ::draft_schema_view_init(db.get(), view_registry_table_name.c_str(),
                                    schema_registry_table_name.c_str());
  ASSERT_EQ(rc, SQLITE_OK) << "Module init failed";
}

void DraftSchemaViewTestBase::TearDown() { db.release(); }

sqlite3 *DraftSchemaViewTestBase::Database() const noexcept { return db.get(); }

static sqlite3 *create_test_memory_database() {
  sqlite3 *db = nullptr;
  int rc = sqlite3_open_v2(
      ":memory:", // Magic filename for in-memory DB
      &db,        // Output: SQLite db handle
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, // Required flags
      nullptr // Name of VFS module (nullptr uses default)
  );

  EXPECT_EQ(rc, SQLITE_OK) << "Error opening database: " << sqlite3_errmsg(db);
  return db;
}

DraftSchemaViewSchemaTestBase::DraftSchemaViewSchemaTestBase(
    const std::string_view schema,
    const std::string_view view_registry_table_name,
    const std::string_view schema_registry_table_name)
    : DraftSchemaViewTestBase(view_registry_table_name,
                              schema_registry_table_name),
      schema(nlohmann::json::parse(schema)) {}

const nlohmann::json &DraftSchemaViewSchemaTestBase::Schema() { return schema; }

void DraftSchemaViewSchemaTestBase::SetUp() {
  DraftSchemaViewTestBase::SetUp();

  auto schema_uri_json = schema.value("$id", nlohmann::json{});
  ASSERT_TRUE(schema_uri_json.is_string()) << "Schema does not specify an $id";

  schema_uri = schema_uri_json.get<std::string>();

  auto rc = draft_schema_view_insert_into_schema_registry(
      Database(), schema_registry_table_name.c_str(), schema_uri.c_str(),
      schema);

  ASSERT_EQ(rc, SQLITE_OK) << "Failed to insert schema to registry";
}

void DraftSchemaViewTestBase::ExpectSqlError(
    const std::string &query, const std::string &expected_msg_substring) const {
  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(Database(), query.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(Database());
    EXPECT_TRUE(err_msg.find(expected_msg_substring) != std::string::npos)
        << "Expected prepare error containing: '" << expected_msg_substring
        << "', but got: '" << err_msg << "'";
    return;
  }

  StatementFinalizer defer_stmt_finalize(stmt);
  rc = sqlite3_step(stmt);

  EXPECT_EQ(rc, SQLITE_ERROR);

  std::string err_msg = sqlite3_errmsg(Database());
  EXPECT_TRUE(err_msg.find(expected_msg_substring) != std::string::npos)
      << "Expected step error containing: '" << expected_msg_substring
      << "', but got: '" << err_msg << "'";
}

void ensure_table_info(sqlite3 *db, const std::string &table_name,
                       const std::vector<ColumnDef> &columns) {

  EXPECT_NE(db, nullptr) << "Database cannot be null";
  if (db == nullptr || table_name.empty()) {
    return;
  }

  char *pragma_table_info_qry =
      sqlite3_mprintf("PRAGMA table_info(%Q)", table_name.c_str());
  EXPECT_NE(pragma_table_info_qry, nullptr)
      << "Failed to format PRAGMA table_info query";

  sqlite3_stmt *pragma_table_info_stmt = nullptr;
  auto rc = sqlite3_prepare_v2(db, pragma_table_info_qry, -1,
                               &pragma_table_info_stmt, nullptr);
  sqlite3_free(pragma_table_info_qry);
  EXPECT_EQ(rc, SQLITE_OK) << "Error preparing " << pragma_table_info_qry
                           << " for " << table_name << ": "
                           << sqlite3_errmsg(db);
  StatementFinalizer defered_pragma_finalize(pragma_table_info_stmt);

  auto column_idx = 0;

  while (rc != SQLITE_DONE) {
    rc = sqlite3_step(pragma_table_info_stmt);
    if (column_idx < columns.size()) {
      EXPECT_EQ(rc, SQLITE_ROW)
          << "Table is missing one or more specified columns";
    } else {
      EXPECT_EQ(rc, SQLITE_DONE) << "Table has unexpected columns.";
    }
    if (rc != SQLITE_ROW) {
      break;
    }
    auto &col = columns.at(column_idx);

    auto num_cols = sqlite3_column_count(pragma_table_info_stmt);
    EXPECT_GE(num_cols, 6) << "Expected 6 columns; " << std::to_string(num_cols)
                           << " were present on PRAGMA table_info";

    auto col_typ = sqlite3_column_type(pragma_table_info_stmt, 0);
    EXPECT_EQ(col_typ, SQLITE_INTEGER)
        << "PRAGMA table_info column 0 (cid) - expected "
           "SQLITE_INTEGER (1), but received "
        << std::to_string(col_typ);
    auto cid = sqlite3_column_int(pragma_table_info_stmt, 0);
    EXPECT_EQ(cid, column_idx)
        << "PRAGMA table_info column 0 (cid) - unexpected column idx: "
        << std::to_string(column_idx);

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 1);
    EXPECT_EQ(col_typ, SQLITE_TEXT)
        << "PRAGMA table_info column 1 (column_name) - expected SQLITE_TEXT "
           "(3), but received "
        << std::to_string(col_typ);
    auto column_name_len = sqlite3_column_bytes(pragma_table_info_stmt, 1);
    EXPECT_GT(column_name_len, 0)
        << "Column name expected non-empty at cid: " << std::to_string(cid);
    auto pColumnName = reinterpret_cast<const char *>(
        sqlite3_column_text(pragma_table_info_stmt, 1));
    EXPECT_NE(pColumnName, nullptr)
        << "Column name expected non-null at cid: " << std::to_string(cid);
    auto column_name = read_string(pColumnName, column_name_len);
    EXPECT_EQ(column_name, col.get_name())
        << "Column name mismatch at cid, " << std::to_string(cid)
        << ": expected " << col.get_name() << ", but received " << column_name;

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 2);
    EXPECT_EQ(col_typ, SQLITE_TEXT)
        << "PRAGMA table_info column 2 (column_type) - expected SQLITE_TEXT "
           "(3), but received "
        << std::to_string(col_typ);
    auto column_type_len = sqlite3_column_bytes(pragma_table_info_stmt, 2);
    EXPECT_GT(column_type_len, 0) << "Expected non-empty column type for "
                                  << column_name << "(cid=" << cid << ")";
    auto pColumnType = reinterpret_cast<const char *>(
        sqlite3_column_text(pragma_table_info_stmt, 2));
    EXPECT_NE(pColumnType, nullptr) << "Expected non-null column type for "
                                    << column_name << "(cid=" << cid << ")";
    auto column_type =
        std::string_view(pColumnType, pColumnType + column_type_len);
    EXPECT_EQ(column_type, col.get_type())
        << "Expected column_type=" << col.get_type() << " for " << column_name
        << ", but received " << column_type;

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 3);
    EXPECT_EQ(col_typ, SQLITE_INTEGER)
        << "PRAGMA table_info column 3 (notnull) - expected SQLITE_INTEGER "
           "(1), but received"
        << std::to_string(col_typ);
    auto notnull = sqlite3_column_int(pragma_table_info_stmt, 3);
    EXPECT_EQ(notnull, col.is_not_null() ? 1 : 0)
        << "Expected " << (col.is_not_null() ? "true" : "false") << " for "
        << column_name << ", but received " << (notnull ? "true" : "false");

    col_typ = sqlite3_column_type(pragma_table_info_stmt, 4);
    auto col_dflt_val = col.get_dflt_value();
    if (col_dflt_val.size() != 0) {
      EXPECT_EQ(col_typ, SQLITE_TEXT)
          << "PRAGMA table_info column 4 (dflt_val) - expected SQLITE_TEXT "
             "(3), but received "
          << std::to_string(col_typ) << " for " << column_name;
    } else {
      EXPECT_TRUE(col_typ == SQLITE_TEXT || col_typ == SQLITE_NULL)
          << "PRAGMA table_info column 4 (dflt_val) - expected SQLITE_TEXT "
             "(3) or SQLITE_NULL (5), but received "
          << std::to_string(col_typ) << " for " << column_name;
      ;
    }
    auto default_value_len = sqlite3_column_bytes(pragma_table_info_stmt, 4);
    auto pDefaultValue = reinterpret_cast<const char *>(
        sqlite3_column_text(pragma_table_info_stmt, 4));
    std::string_view default_value;
    if (pDefaultValue != nullptr && default_value_len > 0) {
      default_value =
          std::string_view(pDefaultValue, pDefaultValue + default_value_len);
    }
    EXPECT_EQ(default_value, col_dflt_val)
        << "Expected dflt_val=" << col.get_dflt_value() << " for "
        << column_name << ", but received " << default_value;

    auto pk = sqlite3_column_int(pragma_table_info_stmt, 5);
    EXPECT_EQ(pk, col.is_pk() ? 1 : 0)
        << "Expected pk=" << (col.is_pk() ? "true" : "false") << " for "
        << column_name << ", but received " << (pk ? "true" : "false");

    column_idx++;
  }

  EXPECT_EQ(column_idx, columns.size())
      << "Table " << table_name << " contained " << std::to_string(column_idx)
      << " columns , but " << std::to_string(columns.size())
      << " were expected";
}
