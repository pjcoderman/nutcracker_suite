#pragma once

#include <gtest/gtest.h>

#include "dsv_common.h"

#include "nlohmann/json.hpp"
#include "sqlite3.h"

#include <memory>

struct SqliteDeleter {
  void operator()(sqlite3 *p) const noexcept { sqlite3_close_v2(p); }
};

using unique_sqlite3_ptr = std::unique_ptr<sqlite3, SqliteDeleter>;

#define DEFAULT_TEST_VIEW_REGISTRY_TABLE_NAME "test_draft_schema_view_registry"
#define DEFAULT_TEST_SCHEMA_REGISTRY_TABLE_NAME                                \
  "test_draft_schema_view_schema_registry"

class DraftSchemaViewTestBase : public ::testing::Test {
private:
  unique_sqlite3_ptr db;

protected:
  const std::string view_registry_table_name;
  const std::string schema_registry_table_name;

  void SetUp() override;

  void TearDown() override;

  sqlite3 *Database() const noexcept;

public:
  DraftSchemaViewTestBase(const std::string_view view_registry_table_name =
                              DEFAULT_TEST_VIEW_REGISTRY_TABLE_NAME,
                          const std::string_view schema_registry_table_name =
                              DEFAULT_TEST_SCHEMA_REGISTRY_TABLE_NAME);
};

class DraftSchemaViewSchemaTestBase : public DraftSchemaViewTestBase {
private:
  nlohmann::json schema;

protected:
  void SetUp() override;

  const nlohmann::json &Schema();
  std::string schema_uri;

public:
  DraftSchemaViewSchemaTestBase(
      const std::string_view schema,
      const std::string_view view_registry_table_name =
          DEFAULT_TEST_VIEW_REGISTRY_TABLE_NAME,
      const std::string_view schema_registry_table_name =
          DEFAULT_TEST_SCHEMA_REGISTRY_TABLE_NAME);
};

void ensure_table_info(sqlite3 *db, const std::string &table_name,
                       const std::vector<ColumnDef> &columns);