#include "dsv_common.h"

void draft_schema_view_vtab_get_key_columns(
    DraftSchemaViewVTab *pVTab, std::vector<std::string_view> &key_columns) {
  std::string_view key_col_list(pVTab->zKeyColumnList);
  for (size_t start = 0, end = 0; start <= key_col_list.size();
       start = end + 1) {
    end = key_col_list.find(',', start);
    if (end == std::string_view::npos) {
      end = key_col_list.size();
    }
    auto key_col = key_col_list.substr(start, end - start);
    if (key_col.empty()) {
      continue;
    }
    key_columns.push_back(key_col);
  }
}

void draft_schema_view_vtab_free(DraftSchemaViewVTab *pVTab) {
  if (pVTab != nullptr) {

    if (pVTab->pOwner != nullptr) {
      pVTab->pOwner->UnregisterConnectedVTable(pVTab);
    }

    if (pVTab->zVTableName != nullptr) {
      sqlite3_free(pVTab->zVTableName);
    }
    if (pVTab->zShadowName != nullptr) {
      sqlite3_free(pVTab->zShadowName);
    }

    if (pVTab->zTargetName != nullptr) {
      sqlite3_free(pVTab->zTargetName);
    }

    if (pVTab->zKeyColumnList != nullptr) {
      sqlite3_free(pVTab->zKeyColumnList);
    }

    sqlite3_free_string_list(pVTab->zColumnList, pVTab->columnCount);
    sqlite3_free(pVTab);
  }
}
