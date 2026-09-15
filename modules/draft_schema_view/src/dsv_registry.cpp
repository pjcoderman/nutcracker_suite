#include "dsv_registry.h"
#include "dsv_common.h"

#include "sqlite3.h"

int draft_schema_view_schema_registry_create_or_update(
    sqlite3 *db, const char *schema_registry_table_name) {
  if (db == nullptr || schema_registry_table_name == nullptr) {
    return SQLITE_ERROR;
  }

  const char *create_schema_registry_fmt = R"(
        CREATE TABLE IF NOT EXISTS %Q(
            schema_uri TEXT NOT NULL PRIMARY KEY,
            schema_json TEXT NOT NULL
        );
    )";

  auto create_schema_registry_qry =
      sqlite3_mprintf(create_schema_registry_fmt, schema_registry_table_name);
  if (create_schema_registry_qry == nullptr) {
    return SQLITE_NOMEM;
  }

  auto rc =
      sqlite3_exec(db, create_schema_registry_qry, nullptr, nullptr, nullptr);
  sqlite3_free(create_schema_registry_qry);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return SQLITE_OK;
}

int draft_schema_view_vtable_registry_create_or_update(
    sqlite3 *db, const char *view_registry_table_name,
    const char *schema_registry_table_name) {
  if (db == nullptr || view_registry_table_name == nullptr ||
      schema_registry_table_name == nullptr) {
    return SQLITE_ERROR;
  }

  const char *create_view_registry_fmt = R"(
        CREATE TABLE IF NOT EXISTS %Q(
            vtable_name TEXT PRIMARY KEY,
            type TEXT CHECK(type IN ('SHADOW', 'FILTER')),
            table_name TEXT NOT NULL,
            schema_uri TEXT NOT NULL,
            key_cols TEXT NOT NULL,
            FOREIGN KEY (schema_uri) 
                REFERENCES %Q (schema_uri) 
                ON DELETE CASCADE
                ON UPDATE CASCADE
        );
    )";

  auto create_view_registry_qry =
      sqlite3_mprintf(create_view_registry_fmt, view_registry_table_name,
                      schema_registry_table_name);
  if (create_view_registry_qry == nullptr) {
    return SQLITE_NOMEM;
  }

  auto rc =
      sqlite3_exec(db, create_view_registry_qry, nullptr, nullptr, nullptr);
  sqlite3_free(create_view_registry_qry);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return SQLITE_OK;
}

int draft_schema_view_insert_into_schema_registry(
    sqlite3 *db, const char *schema_registry_table_name, const char *schema_uri,
    nlohmann::json &schema_json) {
  if (db == nullptr || schema_registry_table_name == nullptr ||
      schema_uri == nullptr || schema_json.is_null()) {
    return SQLITE_ERROR;
  }

  const char *qry_fmt = R"(
        INSERT INTO %s(schema_uri, schema_json) 
        VALUES(:schema_uri, :schema_json);
    )";

  char *query_str = sqlite3_mprintf(qry_fmt, schema_registry_table_name);
  if (query_str == nullptr) {
    return SQLITE_ERROR;
  }

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, query_str, -1, &stmt, nullptr);
  sqlite3_free(query_str);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }
  StatementFinalizer defered_stmt_finalize(stmt);

  auto json_str = schema_json.dump();

  rc = sqlite3_bind_text(stmt, 1, schema_uri, -1, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = sqlite3_bind_text(stmt, 2, json_str.c_str(), -1, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

int draft_schema_view_insert_into_view_registry(
    sqlite3 *db, const char *view_registry_table_name, const char *vtable_name,
    const DraftSchemaViewVTabType type, const char *table_name,
    const char *schema_uri, const char *key_cols_str) {
  if (db == nullptr || view_registry_table_name == nullptr ||
      schema_uri == nullptr || vtable_name == nullptr ||
      table_name == nullptr || key_cols_str == nullptr) {
    return SQLITE_ERROR;
  }

  const char *qry_fmt = R"(
        INSERT INTO %s(vtable_name, type, table_name, schema_uri, key_cols) 
        VALUES(:vtable_name, :type, :table_name, :schema_uri, :key_cols);
    )";

  char *query_str = sqlite3_mprintf(qry_fmt, view_registry_table_name);
  if (query_str == nullptr) {
    return SQLITE_ERROR;
  }

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, query_str, -1, &stmt, nullptr);
  sqlite3_free(query_str);
  if (rc != SQLITE_OK) {
    auto err_msg = sqlite3_errmsg(db);
    return SQLITE_ERROR;
  }
  StatementFinalizer defered_stmt_finalize(stmt);

  rc = sqlite3_bind_text(stmt, 1, vtable_name, -1, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  switch (type) {
  case DRAFT_SCHEMA_VIEW_VTAB_FILTER:
    rc = sqlite3_bind_text(stmt, 2, "FILTER", -1, nullptr);
    break;

  case DRAFT_SCHEMA_VIEW_VTAB_SHADOW:
    rc = sqlite3_bind_text(stmt, 2, "SHADOW", -1, nullptr);
    break;
  default:
    rc = SQLITE_ERROR;
    break;
  };

  rc = sqlite3_bind_text(stmt, 3, table_name, -1, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = sqlite3_bind_text(stmt, 4, schema_uri, -1, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = sqlite3_bind_text(stmt, 5, key_cols_str, -1, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

int draft_schema_viewer_remove_from_view_registry(
    sqlite3 *db, const char *view_registry_table_name,
    const char *vtable_name) {
  if (db == nullptr || view_registry_table_name == nullptr ||
      vtable_name == nullptr) {
    return SQLITE_ERROR;
  }

  const char *qry_fmt = R"(
        DELETE FROM %s
        WHERE vtable_name=:vtable_name;
    )";

  auto qry_str = sqlite3_mprintf(qry_fmt, view_registry_table_name);

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, nullptr, -1, &stmt, nullptr);
  sqlite3_free(qry_str);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }
  StatementFinalizer defer_stmt_finalize(stmt);
  rc = sqlite3_bind_text(stmt, 1, vtable_name, (int)strlen(vtable_name) + 1,
                         nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

// TODO: function to remove from schema registry too - and plan to expose these
// capabilities?
