#include "draft_schema_view.h"
#include "base64.h"
#include "draft_schema_valid.h"
#include "dsv_common.h"
#include "dsv_module.h"
#include "dsv_registry.h"
#include "dsv_sql_helpers.h"
#include "dsv_string_helpers.h"
#include "dsv_vtable_triggers.h"

#include "nlohmann/json-schema.hpp"
#include "nlohmann/json.hpp"
#include "sqlite3.h"

#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

// **** Static utility function declarations ****

// xCreate implementation helpers
static std::string
rewrite_schema_for_vtab_declaration(const std::string_view table_create_sql,
                                    std::string_view newName = "x");
static int
draft_schema_view_populate_index(sqlite3 *db, DraftSchemaViewVTab *pVTab,
                                 std::vector<ColumnDef> &column_defs);
static int
draft_schema_view_create_or_connect(sqlite3 *db, void *pAux, int argc,
                                    const char *const *argv, bool is_create,
                                    sqlite3_vtab **ppVTab, char **pzErr);

// xUpdate implementation helpers
static int draft_schema_view_delete(DraftSchemaViewVTab *pVTab, int argc,
                                    sqlite3_value **argv,
                                    sqlite3_int64 *pRowid);
static int draft_schema_view_insert(DraftSchemaViewVTab *pVTab, int argc,
                                    sqlite3_value **argv,
                                    sqlite3_int64 *pRowid);
static int draft_schema_view_update(DraftSchemaViewVTab *pVTab, int argc,
                                    sqlite3_value **argv,
                                    sqlite3_int64 *pRowid);

// Cursor functions

// **** SQL module function definitions ****

int draft_schema_view_create(sqlite3 *db, void *pAux, int argc,
                             const char *const *argv, sqlite3_vtab **ppVTab,
                             char **pzErr) {
  return draft_schema_view_create_or_connect(db, pAux, argc, argv, true, ppVTab,
                                             pzErr);
}

int draft_schema_view_connect(sqlite3 *db, void *pAux, int argc,
                              const char *const *argv, sqlite3_vtab **ppVTab,
                              char **pzErr) {
  return draft_schema_view_create_or_connect(db, pAux, argc, argv, false,
                                             ppVTab, pzErr);
}

int draft_schema_view_best_index(sqlite3_vtab *pVTab,
                                 sqlite3_index_info *pIdxInfo) {
  // TODO: Better best_index than hardcoded 1000
  pIdxInfo->estimatedCost = 1000.0;
  pIdxInfo->estimatedRows = 1000;
  return SQLITE_OK;
}

int draft_schema_view_disconnect(sqlite3_vtab *pVTab) {
  if (pVTab == nullptr) {
    return SQLITE_ERROR;
  }

  draft_schema_view_vtab_free((DraftSchemaViewVTab *)pVTab);
  return SQLITE_OK;
}

int draft_schema_view_destroy(sqlite3_vtab *pVTab) {
  if (pVTab == nullptr) {
    return SQLITE_ERROR;
  }

  DraftSchemaViewVTab *p = (DraftSchemaViewVTab *)pVTab;

  draft_schema_viewer_remove_from_view_registry(
      p->db, p->pOwner->getViewRegistryTableName().c_str(), p->zVTableName);

  if (p->zShadowName != nullptr) {
    char *drop_sql =
        sqlite3_mprintf("DROP TABLE IF EXISTS %Q;", p->zShadowName);
    sqlite3_exec(p->db, drop_sql, NULL, NULL, NULL);
    sqlite3_free(drop_sql);
  }

  return draft_schema_view_disconnect(pVTab);
}

int draft_schema_view_update(sqlite3_vtab *pVTab, int argc,
                             sqlite3_value **argv, sqlite3_int64 *pRowid) {
  DraftSchemaViewVTab *p = (DraftSchemaViewVTab *)pVTab;

  /* Case 1: DELETE */
  if (argc == 1) {
    return draft_schema_view_delete((DraftSchemaViewVTab *)pVTab, argc, argv,
                                    pRowid);
  }

  /* Case 2: INSERT (argv[0] is NULL, new values start at argv[2]) */
  if (argc > 1 && sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    return draft_schema_view_insert((DraftSchemaViewVTab *)pVTab, argc, argv,
                                    pRowid);
  }

  return draft_schema_view_update((DraftSchemaViewVTab *)pVTab, argc, argv,
                                  pRowid);
}

int draft_schema_view_cursor_open(sqlite3_vtab *pVTab,
                                  sqlite3_vtab_cursor **ppCursor) {
  DraftSchemaViewCursor *pCur =
      (DraftSchemaViewCursor *)sqlite3_malloc(sizeof(DraftSchemaViewCursor));
  if (pCur == nullptr) {
    return SQLITE_NOMEM;
  }

  memset(pCur, 0, sizeof(DraftSchemaViewCursor));
  pCur->base.pVtab = pVTab;

  *ppCursor = (sqlite3_vtab_cursor *)pCur;
  return SQLITE_OK;
}

int draft_schema_view_cursor_close(sqlite3_vtab_cursor *pCursor) {
  if (pCursor == nullptr) {
    return SQLITE_ERROR;
  }

  int rc = SQLITE_OK;
  DraftSchemaViewCursor *pCur = (DraftSchemaViewCursor *)pCursor;
  if (pCur->pStmt != nullptr) {
    rc = sqlite3_finalize(pCur->pStmt);
    if (rc != SQLITE_OK) {
      return rc;
    }
    pCur->pStmt = nullptr;
  }

  sqlite3_free(pCur);
  return rc;
}

int draft_schema_view_cursor_filter(sqlite3_vtab_cursor *pCursor, int idxNum,
                                    const char *idxStr, int argc,
                                    sqlite3_value **argv) {
  if (pCursor == nullptr) {
    return SQLITE_ERROR;
  }

  int rc = SQLITE_OK;
  DraftSchemaViewCursor *pCur = (DraftSchemaViewCursor *)pCursor;
  DraftSchemaViewVTab *p = (DraftSchemaViewVTab *)pCursor->pVtab;

  if (pCur->pStmt) {
    rc = sqlite3_finalize(pCur->pStmt);
    if (rc != SQLITE_OK) {
      return rc;
    }

    pCur->pStmt = nullptr;
  }

  std::vector<std::string_view> key_columns;
  draft_schema_view_vtab_get_key_columns(p, key_columns);

  std::ostringstream builder;
  switch (p->type) {
  case DRAFT_SCHEMA_VIEW_VTAB_SHADOW: {
    builder << "SELECT v.rowid, t.*" << std::endl;
    builder << "FROM " << p->zShadowName << " AS v" << std::endl;
    builder << "INNER JOIN " << p->zTargetName << " AS t" << std::endl;
    builder << "ON " << std::endl;
    for (auto i = 0; i < key_columns.size(); i++) {
      auto &col = key_columns[i];
      builder << "\t";
      if (i != 0) {
        builder << "AND ";
      }
      builder << "v." << col << " = t." << col << std::endl;
    }
  } break;

  case DRAFT_SCHEMA_VIEW_VTAB_FILTER: {
    std::vector<ColumnDef> column_defs;
    rc = get_column_defs(p->db, std::string_view(p->zTargetName), column_defs);
    if (rc != SQLITE_OK) {
      return rc;
    }

    builder << "SELECT t.rowid, t.*" << std::endl;
    builder << "FROM " << p->zTargetName << " AS t" << std::endl;
    builder << "WHERE draft_schema_valid('" << p->zSchemaUri << "'";
    for (auto i = 0; i < column_defs.size(); i++) {
      auto &col = column_defs[i];
      builder << "," << std::endl;
      builder << "\t'" << col.get_name() << "', t." << col.get_name();
    }
    builder << std::endl << ")";
  } break;

  default:
    return SQLITE_ERROR;
  }
  builder << ";";

  auto query = builder.str();
  rc = sqlite3_prepare_v2(p->db, query.c_str(), -1, &pCur->pStmt, NULL);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_step(pCur->pStmt);
  if (rc == SQLITE_ROW) {
    pCur->eof = 0;
    rc = SQLITE_OK;
  } else if (rc == SQLITE_DONE) {
    pCur->eof = 1;
    rc = SQLITE_OK;
  }
  return rc;
}

int draft_schema_view_cursor_next(sqlite3_vtab_cursor *pCursor) {
  DraftSchemaViewCursor *pCur = (DraftSchemaViewCursor *)pCursor;
  int rc = sqlite3_step(pCur->pStmt);
  if (rc == SQLITE_ROW) {
    pCur->eof = 0;
    return SQLITE_OK;
  } else if (rc == SQLITE_DONE) {
    pCur->eof = 1;
    return SQLITE_OK;
  }
  return rc;
}

int draft_schema_view_cursor_eof(sqlite3_vtab_cursor *pCursor) {
  return ((DraftSchemaViewCursor *)pCursor)->eof;
}

int draft_schema_view_cursor_column(sqlite3_vtab_cursor *pCursor,
                                    sqlite3_context *ctx, int iCol) {
  DraftSchemaViewCursor *pCur = (DraftSchemaViewCursor *)pCursor;
  /* Index 0 in the prepared statement is rowid, so columns start at iCol + 1 */
  sqlite3_result_value(ctx, sqlite3_column_value(pCur->pStmt, iCol + 1));
  return SQLITE_OK;
}

int draft_schema_view_cursor_rowid(sqlite3_vtab_cursor *pCursor,
                                   sqlite3_int64 *pRowid) {
  DraftSchemaViewCursor *pCur = (DraftSchemaViewCursor *)pCursor;
  *pRowid = sqlite3_column_int64(pCur->pStmt, 0);
  return SQLITE_OK;
}

void draft_schema_view_module_destroy(void *pClientData) {
  if (pClientData != nullptr) {
    try {
      delete (DraftSchemaViewModuleState *)pClientData;
    } catch (...) {
      // Ignore errors (this is a C callback)
    }
  }
}

// **** Module definition ****

static sqlite3_module draft_schema_viewer_module = {
    .iVersion = 1,
    .xCreate = draft_schema_view_create,
    .xConnect = draft_schema_view_connect,
    .xBestIndex = draft_schema_view_best_index,  // 1st
    .xDisconnect = draft_schema_view_disconnect, // 2nd
    .xDestroy = draft_schema_view_destroy,       // 3rd
    .xOpen = draft_schema_view_cursor_open,
    .xClose = draft_schema_view_cursor_close,
    .xFilter = draft_schema_view_cursor_filter,
    .xNext = draft_schema_view_cursor_next,
    .xEof = draft_schema_view_cursor_eof,
    .xColumn = draft_schema_view_cursor_column,
    .xRowid = draft_schema_view_cursor_rowid,
    .xUpdate = draft_schema_view_update,
    .xBegin = nullptr,
    .xSync = nullptr,
    .xCommit = nullptr,
    .xRollback = nullptr,
    .xFindFunction = nullptr,
    .xRename = nullptr,
    .xSavepoint = nullptr,
    .xRelease = nullptr,
    .xRollbackTo = nullptr,
    .xShadowName = nullptr,
    .xIntegrity = nullptr};

int draft_schema_view_init(sqlite3 *db, const char *view_registry_table_name,
                           const char *schema_registry_table_name) {
  if (db == nullptr || view_registry_table_name == nullptr ||
      schema_registry_table_name == nullptr) {
    return SQLITE_ERROR;
  }

  SQLiteSavepoint savepoint(db, "draft_schema_view_pre_init");

  int rc = draft_schema_view_schema_registry_create_or_update(
      db, schema_registry_table_name);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = draft_schema_view_vtable_registry_create_or_update(
      db, view_registry_table_name, schema_registry_table_name);
  if (rc != SQLITE_OK) {
    return rc;
  }

  DraftSchemaViewModuleState *state = nullptr;
  try {
    state = new DraftSchemaViewModuleState(
        std::string_view(view_registry_table_name),
        std::string_view(schema_registry_table_name));
  } catch (...) {
    return SQLITE_NOMEM;
  }

  rc = draft_schema_valid_func_register(db, state);
  if (rc != SQLITE_OK) {
    delete state;
    return SQLITE_ERROR;
  }

  rc = sqlite3_create_module_v2(db, "draft_schema_view",
                                &draft_schema_viewer_module, state,
                                draft_schema_view_module_destroy);
  if (rc != SQLITE_OK) {
    delete state;
    return SQLITE_ERROR;
  }

  savepoint.commit();
  return SQLITE_OK;
}

// **** Static utility function definitions ****

static std::string
rewrite_schema_for_vtab_declaration(const std::string_view table_create_sql,
                                    std::string_view newName) {
  size_t parenPos = table_create_sql.find('(');
  if (parenPos == std::string::npos) {
    return std::string(table_create_sql); // Fallback / invalid SQL protection
  }
  return std::string("CREATE TABLE ")
      .append(newName)
      .append(table_create_sql.substr(parenPos));
}

static int
draft_schema_view_populate_index(sqlite3 *db, DraftSchemaViewVTab *pVTab,
                                 std::vector<ColumnDef> &column_defs) {

  try {
    std::string_view schema_uri = std::string_view(pVTab->zSchemaUri);

    std::ostringstream insert_query_builder;
    insert_query_builder << "INSERT INTO " << pVTab->zShadowName << std::endl;
    insert_query_builder << "SELECT " << pVTab->zKeyColumnList << std::endl;
    insert_query_builder << "FROM " << pVTab->zTargetName << std::endl;
    insert_query_builder << "WHERE draft_schema_valid(" << std::endl
                         << "\t'" << pVTab->zSchemaUri << "'";

    for (auto it = column_defs.begin(); it != column_defs.end(); ++it) {
      insert_query_builder << "," << std::endl << "\t";
      insert_query_builder << "'" << it->get_name() << "', " << it->get_name()
                           << std::endl;
    }

    insert_query_builder << std::endl << ");";
    auto insert_query_str = insert_query_builder.str();
    int rc =
        sqlite3_exec(db, insert_query_str.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
      return SQLITE_ERROR;
    }

    return SQLITE_OK;
  } catch (...) {
    return SQLITE_ERROR;
  }
}

static std::string_view strip_single_quotes(std::string_view str) {
  if (!str.starts_with("'") || !str.ends_with("'")) {
    return str;
  }
  if (str.length() <= 2) {
    return "";
  }
  return str.substr(1, str.length() - 2);
}

static int
draft_schema_view_create_or_connect(sqlite3 *db, void *pAux, int argc,
                                    const char *const *argv, bool is_create,
                                    sqlite3_vtab **ppVTab, char **pzErr) {

  if (pAux == nullptr) {
    if (pzErr != nullptr) {
      *pzErr = sqlite3_mprintf(
          "pAux must specify a DraftSchemaViewModuleState pointer.");
    }
    return SQLITE_ERROR;
  }

  auto pState = (DraftSchemaViewModuleState *)pAux;

  try {

    // Resolve source table column layout and key constraints via PRAGMA
    // table_info and PRAGMA index_list. Compile validator, prepare row-fetch
    // statement, and write entry to _schema_viewer_bindings. Insert
    // pointer/struct into g_binding_registry[vtab_name]. Generate and execute
    // the underlying _event_rooms_index table and sync triggers.

    if (argc < 4) {
      if (pzErr != nullptr) {
        *pzErr =
            sqlite3_mprintf("Usage: CREATE VIRTUAL TABLE name USING "
                            "draft_schema_viewer(type, target_table_name, "
                            "target_table_name, schema_uri_or_str, [key...])");
      }
      return SQLITE_ERROR;
    }

    // argv[0] module name (draft_schema_viewer)
    // argv[1] database name (e.g. 'main')
    // argv[2] virtual table name
    // argb[3] type ('FILTER' or 'SHADOW')
    // argv[4] target table name
    // argv[5] schema_uri_or_str (uri must exist in schemas table, schema str is
    // added to schemas table) argv[6..] = key column names - if none provided,
    // primary key, rowid, or unique for which the schema is supportive is used
    std::string_view vtable_name(argv[2]), type_str(argv[3]),
        table_name(argv[4]), schema_uri(argv[5]);
    std::vector<std::string_view> key_col_names;

    vtable_name = strip_single_quotes(vtable_name);
    type_str = strip_single_quotes(type_str);
    table_name = strip_single_quotes(table_name);
    schema_uri = strip_single_quotes(schema_uri);
    if (argc > 5) {
      for (auto i = 6; i < argc; i++) {
        key_col_names.push_back(strip_single_quotes(std::string_view(argv[i])));
      }
    }

    std::string vtable_name_str(vtable_name);
    std::string table_name_str(table_name);
    std::string schema_uri_str(schema_uri);

    DraftSchemaViewVTabType vtab_type = DRAFT_SCHEMA_VIEW_VTAB_INVALID;
    if (type_str == "FILTER") {
      vtab_type = DRAFT_SCHEMA_VIEW_VTAB_FILTER;
    } else if (type_str == "SHADOW") {
      vtab_type = DRAFT_SCHEMA_VIEW_VTAB_SHADOW;
    } else {
      if (pzErr != nullptr) {
        *pzErr = sqlite3_mprintf(
            "Unsupported draft_schema_view vtable type: %s", type_str);
      }
      return SQLITE_ERROR;
    }

    int rc = SQLITE_OK;
    std::vector<ColumnDef> column_defs;
    rc = get_column_defs(db, table_name, column_defs);
    if (rc != SQLITE_OK) {
      if (pzErr != nullptr) {
        *pzErr = sqlite3_mprintf(
            "An error occurred retrieving columns of the %s table",
            table_name_str.c_str());
      }
      return SQLITE_ERROR;
    }

    std::vector<std::string_view> pk_column_names;
    std::map<std::string_view, ColumnDef *> column_def_lookup;
    char **pColumnNames =
        (char **)sqlite3_malloc((int)(column_defs.size() * sizeof(char *)));
    if (pColumnNames == nullptr) {
      return SQLITE_NOMEM;
    }
    for (auto i = 0; i < column_defs.size(); i++) {
      auto &col = column_defs[i];
      column_def_lookup[col.get_name()] = &col;
      pColumnNames[i] = sqlite3_mprintf("%s", col.get_name().data());
      if (col.is_pk()) {
        pk_column_names.push_back(col.get_name());
      }
    }

    std::vector<std::vector<std::string>> unique_constraint_cols;
    rc = get_unique_constraints(db, table_name, unique_constraint_cols);
    if (rc != SQLITE_OK) {
      if (pzErr != nullptr) {
        *pzErr = sqlite3_mprintf("Failed to retrieve unique constraints of %s",
                                 table_name);
      }
      return SQLITE_ERROR;
    }

    bool use_pk = key_col_names.size() == 0;
    bool any_unique_constraint_met = use_pk || pk_column_names.size() > 0;

    for (auto &pk_col : pk_column_names) {
      if (use_pk) {
        key_col_names.push_back(pk_col);
      } else {
        bool found = false;
        for (auto &key_col : key_col_names) {
          if (pk_col == key_col) {
            found = true;
            break;
          }
        }
        if (!found) {
          any_unique_constraint_met = false;
          break;
        }
      }
    }

    if (use_pk && key_col_names.size() == 0) {
      use_pk = false;
      any_unique_constraint_met = false;
    }

    if (!any_unique_constraint_met) {
      bool use_first_unique_col_set = key_col_names.size() == 0;

      for (auto &constraint_cols : unique_constraint_cols) {
        bool constraints_met = true;
        for (auto &constraint_col : constraint_cols) {
          if (use_first_unique_col_set) {
            key_col_names.push_back(constraint_col);
          } else {
            bool found = false;
            for (auto &key_col : key_col_names) {
              if (key_col == constraint_col) {
                found = true;
                break;
              }
            }
            if (!found) {
              constraints_met = false;
            }
          }
        }

        if (constraints_met) {
          any_unique_constraint_met = key_col_names.size() > 0;
          if (any_unique_constraint_met) {
            break;
          }
        }
      }
    }

    if (!any_unique_constraint_met &&
        (vtab_type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW)) {

      sqlite3_free_string_list(pColumnNames, column_defs.size());

      // Nothing to base a lookup table on reliably
      if (pzErr != nullptr) {
        *pzErr =
            sqlite3_mprintf("Failed to determine foreign keys for the %s table",
                            table_name_str.c_str());
      }
      return SQLITE_ERROR;
    }

    std::string key_cols_str = join_with_comma(key_col_names);

    bool is_first = true;
    std::ostringstream shadow_table_def;

    SQLiteSavepoint savepoint(db, "draft_schema_view_preconnect");

    // Get the SQL declaration of the original table
    std::string table_sql;
    rc = sqlite3_get_table_sql(db, table_name, table_sql);
    if (rc != SQLITE_OK) {
      sqlite3_free_string_list(pColumnNames, column_defs.size());
      if (pzErr != nullptr) {
        *pzErr = sqlite3_mprintf(
            "Failed to retrieve the SQL declaration of the %s table",
            table_name_str.c_str());
      }
      return rc;
    }

    // Register schema with the SQLite parser
    auto vtable_sql =
        rewrite_schema_for_vtab_declaration(table_sql, vtable_name);
    rc = sqlite3_declare_vtab(db, vtable_sql.c_str());
    if (rc != SQLITE_OK) {
      sqlite3_free_string_list(pColumnNames, column_defs.size());
      if (pzErr != nullptr) {
        *pzErr = sqlite3_mprintf("An error occurred declaring the %s vtable",
                                 vtable_name_str.c_str());
      }
      return rc;
    }

    rc = draft_schema_view_insert_into_view_registry(
        db, pState->getViewRegistryTableName().c_str(), vtable_name_str.c_str(),
        vtab_type, table_name_str.c_str(), schema_uri_str.c_str(),
        key_cols_str.c_str());

    if (rc != SQLITE_OK) {
      sqlite3_free_string_list(pColumnNames, column_defs.size());
      if (pzErr != nullptr) {
        *pzErr = sqlite3_mprintf("An error occurred registering the %s vtable",
                                 vtable_name.data());
      }
      return rc;
    }

    // Allocate our vtab structure
    DraftSchemaViewVTab *pVTab =
        (DraftSchemaViewVTab *)sqlite3_malloc(sizeof(DraftSchemaViewVTab));
    if (!pVTab) {
      sqlite3_free_string_list(pColumnNames, column_defs.size());
      return SQLITE_NOMEM;
    }
    memset(pVTab, 0, sizeof(DraftSchemaViewVTab));

    pVTab->db = db;
    pVTab->zVTableName = sqlite3_mprintf("%s", vtable_name_str.c_str());
    pVTab->zShadowName = sqlite3_mprintf("%s_shadow", vtable_name_str.c_str());
    pVTab->zTargetName = sqlite3_mprintf("%s", table_name_str.c_str());
    pVTab->zSchemaUri = sqlite3_mprintf("%s", schema_uri_str.c_str());
    pVTab->type = vtab_type;
    pVTab->zKeyColumnList = sqlite3_mprintf("%s", key_cols_str.c_str());
    pVTab->columnCount = column_defs.size();
    pVTab->zColumnList = pColumnNames;

    if (vtab_type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW) {
      shadow_table_def << "CREATE TABLE IF NOT EXISTS " << vtable_name
                       << "_shadow(";

      for (auto &key_col : key_col_names) {
        if (is_first) {
          is_first = false;
          shadow_table_def << std::endl << "\t";
        } else {
          shadow_table_def << "," << std::endl << "\t";
        }
        auto key_col_def_it = column_def_lookup.find(key_col);
        if (key_col_def_it == column_def_lookup.end()) {
          if (pzErr != nullptr) {
            *pzErr = sqlite3_mprintf(
                "The foreign key column, %s, was not found within the %s table",
                key_col.data(), vtable_name.data());
          }
          return SQLITE_ERROR;
        }
        auto key_col_def = key_col_def_it->second;

        shadow_table_def << key_col_def->get_name() << " "
                         << key_col_def->get_type();
        if (key_col_def->is_not_null()) {
          shadow_table_def << " NOT NULL";
        }
        if (key_col_names.size() == 1) {
          shadow_table_def << " PRIMARY KEY";
        }
      }

      if (key_col_names.size() > 1) {
        shadow_table_def << "," << std::endl << "\tPRIMARY KEY(";
        is_first = true;
        for (auto &key_col : key_col_names) {
          if (is_first) {
            is_first = false;
            shadow_table_def << std::endl << "\t\t";
          } else {
            shadow_table_def << "," << std::endl << "\t\t";
          }
          shadow_table_def << key_col;
        }
        shadow_table_def << std::endl << "\t)";
      }

      shadow_table_def << "," << std::endl;
      shadow_table_def << "\tFOREIGN KEY (";
      is_first = true;
      for (auto &key_col : key_col_names) {
        if (!is_first) {
          shadow_table_def << ",";
        } else {
          is_first = false;
        }
        shadow_table_def << std::endl << "\t\t" << key_col;
      }
      shadow_table_def << std::endl << "\t)" << std::endl;
      shadow_table_def << "\t\tREFERENCES " << table_name << "(";
      is_first = true;
      for (auto &key_col : key_col_names) {
        if (!is_first) {
          shadow_table_def << ",";
        } else {
          is_first = false;
        }
        shadow_table_def << std::endl << "\t\t\t" << key_col;
      }
      shadow_table_def << std::endl << "\t\t)";
      shadow_table_def << std::endl << "\t\tON DELETE CASCADE";
      // NOTE: Update cascade is handled by triggers
      // shadow_table_def << std::endl << "\t\tON UPDATE CASCADE";
      shadow_table_def << std::endl << ");";

      /* Create the real backing B-tree table in the same DB */
      auto shadow_create = shadow_table_def.str();

      rc = sqlite3_exec(db, shadow_create.c_str(), NULL, NULL, pzErr);
      if (rc != SQLITE_OK) {
        draft_schema_view_vtab_free(pVTab);
        if (pzErr != nullptr) {
          auto err_msg = sqlite3_errmsg(db);
          (*pzErr) =
              sqlite3_mprintf("Error creating shadow table: %s", err_msg);
        }
        return rc;
      }

      rc = draft_schema_view_create_triggers(pVTab);
      if (rc != SQLITE_OK) {
        draft_schema_view_vtab_free(pVTab);
        if (pzErr != nullptr) {
          auto err_msg = sqlite3_errmsg(db);
          *pzErr =
              sqlite3_mprintf("Error setting up vtable %Q on table %Q",
                              vtable_name_str.c_str(), table_name_str.c_str());
        }
        return rc;
      }
    }

    pState->RegisterConnectedVTable(pVTab);

    if (is_create && (vtab_type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW)) {
      rc = draft_schema_view_populate_index(db, pVTab, column_defs);
      if (rc != SQLITE_OK) {

        // Cleanup resources
        draft_schema_view_vtab_free(pVTab);

        if (pzErr != nullptr) {
          (*pzErr) = sqlite3_mprintf("Error populating index table");
        }
        return SQLITE_ERROR;
      }
    }

    savepoint.commit();
    *ppVTab = (sqlite3_vtab *)pVTab;
    return SQLITE_OK;
  } catch (std::exception &e) {
    if (pzErr != nullptr) {
      (*pzErr) = sqlite3_mprintf("An internal error occurred: %s", e.what());
    }
    return SQLITE_ERROR;
  }
}

static int draft_schema_view_delete(DraftSchemaViewVTab *pVTab, int argc,
                                    sqlite3_value **argv,
                                    sqlite3_int64 *pRowid) {
  if (pVTab == nullptr) {
    return SQLITE_ERROR;
  }
  if (argv == nullptr || argc < 1) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "draft_schema_view xDelete requires at least one argument.");
    return SQLITE_ERROR;
  }

  sqlite3_stmt *stmt = nullptr;

  const char *delete_from_target_fmt = R"(
        DELETE FROM %Q 
        WHERE (%s) = (
            SELECT %s FROM %Q WHERE rowid = ?1
        );
    )";
  const char *errmsg = nullptr;
  char *delete_from_target = sqlite3_mprintf(
      delete_from_target_fmt, pVTab->zTargetName, pVTab->zShadowName);
  int rc = sqlite3_prepare_v2(pVTab->db, delete_from_target, -1, &stmt, NULL);
  sqlite3_free(delete_from_target);
  if (rc != SQLITE_OK) {
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return rc;
  }
  {
    StatementFinalizer defer_delete_from_target_finalize(stmt);
    rc = sqlite3_bind_value(stmt, 1, argv[0]);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return SQLITE_ERROR;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return SQLITE_ERROR;
    }
  }

  if (pVTab->type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW) {
    const char *delete_from_shadow_fmt = R"(
            DELETE FROM %Q
            WHERE rowid = ?;
        )";

    char *delete_from_shadow =
        sqlite3_mprintf(delete_from_shadow_fmt, pVTab->zShadowName);
    rc = sqlite3_prepare_v2(pVTab->db, delete_from_shadow, -1, &stmt, NULL);
    sqlite3_free(delete_from_shadow);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return rc;
    }
    {
      StatementFinalizer defer_delete_from_shadow_finalize(stmt);
      rc = sqlite3_bind_value(stmt, 1, argv[0]);
      if (rc != SQLITE_OK) {
        errmsg = sqlite3_errmsg(pVTab->db);
        pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
        return SQLITE_ERROR;
      }
      rc = sqlite3_step(stmt);
      if (rc != SQLITE_OK) {
        errmsg = sqlite3_errmsg(pVTab->db);
        pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
        return rc;
      }
    }
  }
  return SQLITE_OK;
}

static int draft_schema_view_insert(DraftSchemaViewVTab *pVTab, int argc,
                                    sqlite3_value **argv,
                                    sqlite3_int64 *pRowid) {

  if (pVTab == nullptr) {
    return SQLITE_ERROR;
  }

  if (argv == nullptr || argc < 3) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "draft_schema_view xUpdate requires at least 3 SQL arguments.");
    return SQLITE_ERROR;
  }

  if (pVTab->pOwner == nullptr) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "The virtual table was not properly registered with its owner.");
    return SQLITE_ERROR;
  }

  sqlite3_value **colv = &argv[2];
  int colc = argc - 2;

  if (colc != pVTab->columnCount) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "draft_schema_view xUpdate unexpected column value count: %i.", argc);
    return SQLITE_ERROR;
  }

  // Split the key columns specification to string views
  std::vector<std::string_view> key_columns;
  draft_schema_view_vtab_get_key_columns(pVTab, key_columns);

  // Retrieve the column definitions for the target table
  std::vector<ColumnDef> column_defs;
  int rc = get_column_defs(pVTab->db, std::string_view(pVTab->zTargetName),
                           column_defs);
  if (rc != SQLITE_OK) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "Failed to get column definitions for %s", pVTab->zTargetName);
    return rc;
  }

  // Build lookup using non-const pointers to match ColumnDef's non-const
  // accessors
  std::map<std::string_view, ColumnDef *> column_def_lookup;
  for (auto &col : column_defs) {
    column_def_lookup[col.get_name()] = &col;
  }

  std::vector<sqlite3_value *> key_values;
  for (auto key_col_name : key_columns) {
    auto key_col_it = column_def_lookup.find(key_col_name);
    if (key_col_it == column_def_lookup.end()) {
      std::string key_col_name_str(key_col_name);
      pVTab->base.zErrMsg = sqlite3_mprintf(
          "Key column, '%s', specified for vtable '%s', was not found within "
          "'%s'",
          key_col_name_str.c_str(), pVTab->zVTableName, pVTab->zTargetName);
      return SQLITE_ERROR;
    }
    auto key_col = key_col_it->second;
    int idx = key_col->get_ordinal();
    if (idx < 0 || idx >= colc) {
      std::string key_col_name_str(key_col_name);
      pVTab->base.zErrMsg =
          sqlite3_mprintf("No value was provided for key column, '%s', on "
                          "insertion to vtable '%s'",
                          key_col_name_str.c_str(), pVTab->zVTableName);
      return SQLITE_ERROR;
    }
    key_values.push_back(colv[idx]);
  }

  // Validate JSON draft schema on incoming row data
  auto pSchema =
      pVTab->pOwner->GetSchema(pVTab->db, std::string_view(pVTab->zSchemaUri));
  if (pSchema != nullptr) {
    nlohmann::json row_json = construct_json_row(pVTab, colc, colv);
    try {
      pSchema->validator.validate(row_json);
    } catch (std::exception &e) {
      pVTab->base.zErrMsg =
          sqlite3_mprintf("Draft schema validation failure: %s", e.what());
      return SQLITE_CONSTRAINT;
    }
  }

  std::ostringstream insert_query_builder;
  insert_query_builder << "INSERT INTO " << pVTab->zTargetName << "(";
  bool is_first = true;
  for (auto col : column_defs) {
    if (is_first) {
      insert_query_builder << std::endl << "\t";
      is_first = false;
    } else {
      insert_query_builder << "," << std::endl << "\t";
    }
    insert_query_builder << col.get_name();
  }
  insert_query_builder << std::endl << ")" << std::endl;
  insert_query_builder << "VALUES (";
  is_first = true;
  for (auto i = 0; i < colc; i++) {
    if (is_first) {
      insert_query_builder << std::endl << "\t";
      is_first = false;
    } else {
      insert_query_builder << "," << std::endl << "\t";
    }
    insert_query_builder << "?" << std::to_string(i + 1);
  }
  insert_query_builder << std::endl << ");";

  const char *errmsg;
  sqlite3_stmt *insert_stmt = nullptr;
  auto insert_query = insert_query_builder.str();
  rc = sqlite3_prepare_v2(pVTab->db, insert_query.c_str(), -1, &insert_stmt,
                          nullptr);
  if (rc != SQLITE_OK) {
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return rc;
  }

  StatementFinalizer deferred_insert_finalize(insert_stmt);
  for (auto c = 0; c < colc; c++) {
    auto t = sqlite3_value_type(colv[c]);
    rc = sqlite3_bind_value(insert_stmt, c + 1, colv[c]);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return SQLITE_ERROR;
    }
  }

  rc = sqlite3_step(insert_stmt);
  if (rc != SQLITE_DONE) {
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return SQLITE_ERROR;
  }

  std::ostringstream select_query_builder;
  select_query_builder << "SELECT rowid " << std::endl;
  if (pVTab->type == DRAFT_SCHEMA_VIEW_VTAB_SHADOW) {
    select_query_builder << "FROM " << pVTab->zShadowName << std::endl;
  } else {
    select_query_builder << "FROM " << pVTab->zTargetName << std::endl;
  }
  select_query_builder << "WHERE ";
  for (auto i = 0; i < key_columns.size(); i++) {
    auto key_col = key_columns[i];
    if (i == 0) {
      select_query_builder << std::endl << "\t";
    } else {
      select_query_builder << std::endl << "\tAND ";
    }
    select_query_builder << key_col << " = ?" << std::to_string(i + 1);
  }

  auto select_query = select_query_builder.str();
  sqlite3_stmt *select_stmt = nullptr;
  rc = sqlite3_prepare_v2(pVTab->db, select_query.c_str(), -1, &select_stmt,
                          nullptr);
  if (rc != SQLITE_OK) {
    // TODO: consider not returning error for this and/or checking for lack of
    // rowid as the cause also consider potentially checking for WITHOUT rowid
    // on create or connect (store in pVTab)
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return SQLITE_ERROR;
  }
  StatementFinalizer deferred_select_finalize(select_stmt);

  for (auto k = 0; k < key_values.size(); k++) {
    rc = sqlite3_bind_value(select_stmt, k + 1, key_values[k]);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return SQLITE_ERROR;
    }
  }

  rc = sqlite3_step(select_stmt);
  if (rc != SQLITE_ROW) {
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return SQLITE_ERROR;
  }

  *pRowid = sqlite3_column_int64(select_stmt, 0);
  return SQLITE_OK;
}

static int draft_schema_view_update(DraftSchemaViewVTab *pVTab, int argc,
                                    sqlite3_value **argv,
                                    sqlite3_int64 *pRowid) {

  if (pVTab == nullptr) {
    return SQLITE_ERROR;
  }

  if (argv == nullptr || argc < 3) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "draft_schema_view xUpdate requires at least 3 SQL arguments.");
    return SQLITE_ERROR;
  }

  if (pVTab->pOwner == nullptr) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "The virtual table was not properly registered with its owner.");
    return SQLITE_ERROR;
  }

  sqlite3_value **colv = &argv[2];
  int colc = argc - 2;

  if (colc != pVTab->columnCount) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "draft_schema_view xUpdate unexpected column value count: %i.", argc);
    return SQLITE_ERROR;
  }

  // Split the key columns specification to string views
  std::vector<std::string_view> key_columns;
  draft_schema_view_vtab_get_key_columns(pVTab, key_columns);

  // Retrieve the column definitions for the target table
  std::vector<ColumnDef> column_defs;
  int rc = get_column_defs(pVTab->db, std::string_view(pVTab->zTargetName),
                           column_defs);
  if (rc != SQLITE_OK) {
    pVTab->base.zErrMsg = sqlite3_mprintf(
        "Failed to get column definitions for %s", pVTab->zTargetName);
    return rc;
  }

  // Build lookup using non-const pointers to match ColumnDef's non-const
  // accessors
  std::map<std::string_view, ColumnDef *> column_def_lookup;
  for (auto &col : column_defs) {
    column_def_lookup[col.get_name()] = &col;
  }

  std::vector<sqlite3_value *> key_values;
  bool key_columns_updated = false;

  for (auto key_col_name : key_columns) {
    auto key_col_it = column_def_lookup.find(key_col_name);
    if (key_col_it == column_def_lookup.end()) {
      std::string key_col_name_str(key_col_name);
      pVTab->base.zErrMsg = sqlite3_mprintf(
          "Key column, '%s', specified for vtable '%s', was not found within "
          "'%s'",
          key_col_name_str.c_str(), pVTab->zVTableName, pVTab->zTargetName);
      return SQLITE_ERROR;
    }
    auto key_col = key_col_it->second;
    if (key_col->get_ordinal() < colc) {
      auto key_val = colv[key_col->get_ordinal()];

      if (sqlite3_value_nochange(key_val)) {
        key_values.push_back(key_val);
      } else {
        key_values.clear();
        key_columns_updated = true;
        break;
      }
    }
  }

  sqlite3_stmt *current_key_values_stmt = nullptr;
  const char *errmsg = nullptr;

  // If key columns changed, look up original values from the shadow table
  if (key_columns_updated) {
    const char *current_key_values_sql_fmt = R"(
            SELECT %s
            FROM %Q
            WHERE rowId=?1;
        )";

    char *current_key_values_sql = nullptr;
    switch (pVTab->type) {
    case DRAFT_SCHEMA_VIEW_VTAB_SHADOW:
      current_key_values_sql =
          sqlite3_mprintf(current_key_values_sql_fmt, pVTab->zKeyColumnList,
                          pVTab->zShadowName);
      break;
    case DRAFT_SCHEMA_VIEW_VTAB_FILTER:
      current_key_values_sql =
          sqlite3_mprintf(current_key_values_sql_fmt, pVTab->zKeyColumnList,
                          pVTab->zTargetName);
      break;
    default:
      pVTab->base.zErrMsg =
          sqlite3_mprintf("Unrecognized draft_schema_view type: %i",
                          static_cast<int>(pVTab->type));
      return SQLITE_ERROR;
    };

    if (current_key_values_sql == nullptr) {
      return SQLITE_NOMEM;
    }

    rc = sqlite3_prepare_v2(pVTab->db, current_key_values_sql, -1,
                            &current_key_values_stmt, nullptr);
    sqlite3_free(current_key_values_sql);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return rc;
    }

    // argv[0] contains this vtable row's ID, which matches the shadow table's
    // rowid
    rc = sqlite3_bind_value(current_key_values_stmt, 1, argv[0]);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      sqlite3_finalize(current_key_values_stmt);
      return rc;
    }

    rc = sqlite3_step(current_key_values_stmt);
    if (rc != SQLITE_ROW) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      sqlite3_finalize(current_key_values_stmt);
      return SQLITE_NOTFOUND;
    }

    // sqlite3_column_* is 0-indexed
    for (size_t k = 0; k < key_columns.size(); k++) {
      sqlite3_value *key_val =
          sqlite3_column_value(current_key_values_stmt, static_cast<int>(k));
      key_values.push_back(key_val);
    }
  }

  StatementFinalizer current_key_values_stmt_defer_finalize(
      current_key_values_stmt);

  // Validate JSON draft schema on incoming row data
  auto pSchema =
      pVTab->pOwner->GetSchema(pVTab->db, std::string_view(pVTab->zSchemaUri));
  if (pSchema != nullptr) {
    nlohmann::json row_json = construct_json_row(pVTab, colc, colv);
    try {
      pSchema->validator.validate(row_json);
    } catch (std::exception &e) {
      pVTab->base.zErrMsg =
          sqlite3_mprintf("Draft schema validation failure: %s", e.what());
      return SQLITE_CONSTRAINT;
    }
  }

  // Construct UPDATE query for the target table
  std::ostringstream update_sql_builder;
  update_sql_builder << "UPDATE " << pVTab->zTargetName << std::endl;
  update_sql_builder << "SET";
  bool is_first = true;
  int ord = 1;
  for (auto &col : column_defs) {
    if (is_first) {
      update_sql_builder << std::endl << "\t";
      is_first = false;
    } else {
      update_sql_builder << "," << std::endl << "\t";
    }
    update_sql_builder << col.get_name() << " = ?" << std::to_string(ord++);
  }

  update_sql_builder << std::endl << "WHERE";
  is_first = true;
  for (size_t k = 0; k < key_columns.size(); k++) {
    auto &key_col = key_columns[k];
    if (is_first) {
      is_first = false;
      update_sql_builder << std::endl << "\t";
    } else {
      update_sql_builder << " AND " << std::endl << "\t";
    }
    update_sql_builder << key_col << " = ?" << std::to_string(ord++);
  }
  update_sql_builder << ";";

  auto update_sql = update_sql_builder.str();

  // Prepare and execute against target table
  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(pVTab->db, update_sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return rc;
  }
  StatementFinalizer deferred_update_finalize(stmt);

  ord = 1;
  for (int i = 0; i < colc; i++) {
    rc = sqlite3_bind_value(stmt, ord++, colv[i]);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return SQLITE_ERROR;
    }
  }

  // Bind WHERE key parameters (statement remains open so pointers remain valid)
  for (size_t k = 0; k < key_values.size(); k++) {
    rc = sqlite3_bind_value(stmt, ord++, key_values[k]);
    if (rc != SQLITE_OK) {
      errmsg = sqlite3_errmsg(pVTab->db);
      pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
      return SQLITE_ERROR;
    }
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    errmsg = sqlite3_errmsg(pVTab->db);
    pVTab->base.zErrMsg = sqlite3_mprintf("%s", errmsg);
    return rc;
  }

  return SQLITE_OK;
}
