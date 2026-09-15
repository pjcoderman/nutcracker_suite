#include "dsv_vtable_triggers.h"
#include "dsv_common.h"
#include "dsv_sql_helpers.h"

#include <iostream>
#include <sstream>
#include <vector>


int draft_schema_view_create_insert_valid_trigger(
    DraftSchemaViewVTab *pVTab,
    const std::vector<std::string_view> &key_columns,
    std::vector<ColumnDef> &column_defs) {

  std::ostringstream insert_trigger_builder;
  insert_trigger_builder << "CREATE TRIGGER draft_schema_view_"
                         << pVTab->zVTableName << "_insert" << std::endl;
  insert_trigger_builder << "AFTER INSERT ON " << pVTab->zTargetName
                         << std::endl;
  insert_trigger_builder << "WHEN draft_schema_valid('" << pVTab->zSchemaUri
                         << "'";
  for (auto c = 0; c < column_defs.size(); c++) {
    insert_trigger_builder << "," << std::endl << "\t";
    insert_trigger_builder << "'" << column_defs[c].get_name() << "', NEW."
                           << column_defs[c].get_name();
  }
  insert_trigger_builder << ")" << std::endl;
  insert_trigger_builder << "BEGIN" << std::endl;
  insert_trigger_builder << "\tINSERT INTO " << pVTab->zShadowName << "("
                         << pVTab->zKeyColumnList << ")" << std::endl;
  insert_trigger_builder << "\tVALUES (";
  for (auto k = 0; k < key_columns.size(); k++) {
    if (k == 0) {
      insert_trigger_builder << std::endl << "\t\t";
    } else {
      insert_trigger_builder << "," << std::endl << "\t\t";
    }
    insert_trigger_builder << "NEW." << key_columns[k];
  }
  insert_trigger_builder << std::endl << "\t);";
  insert_trigger_builder << std::endl;
  insert_trigger_builder << "END;";

  auto insert_trigger_qry = insert_trigger_builder.str();
  auto rc = sqlite3_exec(pVTab->db, insert_trigger_qry.c_str(), nullptr,
                         nullptr, nullptr);
  return rc;
}

int draft_schema_view_create_update_valid_trigger(
    DraftSchemaViewVTab *pVTab,
    const std::vector<std::string_view> &key_columns,
    std::vector<ColumnDef> &column_defs) {

  std::ostringstream builder;
  builder << "CREATE TRIGGER draft_schema_view_" << pVTab->zVTableName
          << "_upd_valid" << std::endl;
  builder << "AFTER UPDATE ON " << pVTab->zTargetName << std::endl;
  builder << "WHEN draft_schema_valid('" << pVTab->zSchemaUri << "'";
  for (size_t c = 0; c < column_defs.size(); c++) {
    builder << "," << std::endl << "\t";
    builder << "'" << column_defs[c].get_name() << "', NEW."
            << column_defs[c].get_name();
  }
  builder << ")" << std::endl;
  builder << "BEGIN" << std::endl;

  // Purge old key if key mutated
  builder << "\tDELETE FROM " << pVTab->zShadowName << std::endl;
  builder << "\tWHERE (";
  for (size_t k = 0; k < key_columns.size(); k++) {
    builder << std::endl << "\t\t";
    if (k > 0)
      builder << " AND ";
    builder << key_columns[k] << " = OLD." << key_columns[k];
  }
  builder << "\t)" << std::endl;
  builder << "\t  AND (";
  for (size_t k = 0; k < key_columns.size(); k++) {
    if (k > 0)
      builder << " OR ";
    builder << "OLD." << key_columns[k] << " IS NOT NEW." << key_columns[k];
  }
  builder << "\t);" << std::endl;

  // Insert new key into shadow
  builder << "\tINSERT OR IGNORE INTO " << pVTab->zShadowName << "("
          << pVTab->zKeyColumnList << ")" << std::endl;
  builder << "\tVALUES (";
  for (size_t k = 0; k < key_columns.size(); k++) {
    if (k > 0)
      builder << ", ";
    builder << std::endl << "\t";
    builder << "NEW." << key_columns[k];
  }
  builder << ");" << std::endl;
  builder << "END;";

  return sqlite3_exec(pVTab->db, builder.str().c_str(), nullptr, nullptr,
                      nullptr);
}

int draft_schema_view_create_update_invalid_trigger(
    DraftSchemaViewVTab *pVTab,
    const std::vector<std::string_view> &key_columns,
    std::vector<ColumnDef> &column_defs) {

  std::ostringstream builder;
  builder << "CREATE TRIGGER draft_schema_view_" << pVTab->zVTableName
          << "_upd_invalid" << std::endl;
  builder << "AFTER UPDATE ON " << pVTab->zTargetName << std::endl;
  builder << "WHEN NOT draft_schema_valid('" << pVTab->zSchemaUri << "'";
  for (size_t c = 0; c < column_defs.size(); c++) {
    builder << "," << std::endl << "\t";
    builder << "'" << column_defs[c].get_name() << "', NEW."
            << column_defs[c].get_name();
  }
  builder << ")" << std::endl;
  builder << "BEGIN" << std::endl;

  // Purge invalid key from shadow
  builder << "\tDELETE FROM " << pVTab->zShadowName << std::endl;
  builder << "\tWHERE ";
  for (size_t k = 0; k < key_columns.size(); k++) {
    if (k > 0)
      builder << " AND ";
    builder << key_columns[k] << " = OLD." << key_columns[k];
  }
  builder << ";" << std::endl;
  builder << "END;";

  return sqlite3_exec(pVTab->db, builder.str().c_str(), nullptr, nullptr,
                      nullptr);
}

int draft_schema_view_create_triggers(DraftSchemaViewVTab *pVTab) {
  std::vector<std::string_view> key_columns;
  draft_schema_view_vtab_get_key_columns(pVTab, key_columns);

  std::vector<ColumnDef> column_defs;
  int rc = get_column_defs(pVTab->db, std::string_view(pVTab->zTargetName),
                           column_defs);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = draft_schema_view_create_insert_valid_trigger(pVTab, key_columns,
                                                     column_defs);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = draft_schema_view_create_update_valid_trigger(pVTab, key_columns,
                                                     column_defs);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = draft_schema_view_create_update_invalid_trigger(pVTab, key_columns,
                                                       column_defs);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}
