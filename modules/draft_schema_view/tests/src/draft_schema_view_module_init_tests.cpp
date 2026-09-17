#include "draft_schema_view_test_base.hpp"
#include "dsv_common.h"

class DraftSchemaViewModuleInitTests : public DraftSchemaViewTestBase {

public:
  DraftSchemaViewModuleInitTests() : DraftSchemaViewTestBase() {}
};

TEST_F(DraftSchemaViewModuleInitTests, init_creates_view_registry_table) {

  std::vector<ColumnDef> expected_columns;
  expected_columns.push_back(
      ColumnDef(0, "schema_uri", "TEXT", true, "", true));
  expected_columns.push_back(
      ColumnDef(1, "schema_json", "TEXT", true, "", false));

  ASSERT_NO_FATAL_FAILURE(ensure_table_info(
      Database(), schema_registry_table_name, expected_columns));
}

TEST_F(DraftSchemaViewModuleInitTests, init_creates_schema_registry_table) {
  std::vector<ColumnDef> expected_columns;
  expected_columns.push_back(
      ColumnDef(0, "schema_uri", "TEXT", true, "", true));
  expected_columns.push_back(
      ColumnDef(1, "schema_json", "TEXT", true, "", false));
  ensure_table_info(Database(), schema_registry_table_name, expected_columns);
}