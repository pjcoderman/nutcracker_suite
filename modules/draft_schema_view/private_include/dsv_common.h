#pragma once

#include "dsv_string_helpers.h"
#include "nlohmann/json-schema.hpp"
#include "nlohmann/json.hpp"
#include "sqlite3.h"


#include <iostream>
#include <set>

// **** Struct declarations ****
typedef struct DraftSchemaViewVTab DraftSchemaViewVTab;
typedef struct DraftSchemaViewCursor DraftSchemaViewCursor;

// **** Class Declarations ****
class DraftSchemaViewModuleState;
class StatementFinalizer;
class ColumnDef;
class CachedDraftSchema;

/**
 * @brief Provides automatic rollback to a save point absent a commit.
 *
 */
class SQLiteSavepoint {
public:
  /**
   * @brief Construct a new SQLiteSavepoint object
   *
   * @param db A pointer to the database to which the savepoint applies.
   * @param name Specifies the name of the savepoint.
   */
  explicit SQLiteSavepoint(sqlite3 *db, const char *name)
      : db_(db), name_(name), active_(false) {
    char *zSql = sqlite3_mprintf("SAVEPOINT %q;", name_);
    if (zSql) {
      int rc = sqlite3_exec(db_, zSql, nullptr, nullptr, nullptr);
      sqlite3_free(zSql);
      if (rc == SQLITE_OK) {
        active_ = true;
      }
    }
  }

  // Prevent copying so it doesn't finalize twice
  SQLiteSavepoint(const SQLiteSavepoint &) = delete;
  SQLiteSavepoint &operator=(const SQLiteSavepoint &) = delete;

  /**
   * @brief Destroy the SQLiteSavepoint object.  Unless committed, rolls back to
   * the established savepoint.
   *
   */
  ~SQLiteSavepoint() {
    if (active_) {
      char *zSql = sqlite3_mprintf("ROLLBACK TO %q; RELEASE %q;", name_, name_);
      if (zSql) {
        sqlite3_exec(db_, zSql, nullptr, nullptr, nullptr);
        sqlite3_free(zSql);
      }
    }
  }

  /**
   * @brief Commits the changes for, and releases, the savepoint.
   *
   * @return int
   */
  int commit() {
    if (!active_)
      return SQLITE_ERROR;
    char *zSql = sqlite3_mprintf("RELEASE %q;", name_);
    if (!zSql)
      return SQLITE_NOMEM;
    int rc = sqlite3_exec(db_, zSql, nullptr, nullptr, nullptr);
    sqlite3_free(zSql);
    if (rc == SQLITE_OK) {
      active_ = false;
    }
    return rc;
  }

  /**
   * @brief Indicates whether or not the savepoint is still active
   * (uncommitted).
   *
   * @return true The changes for the savepoint have not been committed (and
   * will be rolled back on destruction).
   * @return false The changes for the savepoint have been committed (and will
   * remain after destruction).
   */
  bool is_active() const { return active_; }

private:
  sqlite3 *db_;
  const char *name_;
  bool active_;
};

/**
 * @brief Provides RAII finalization for an sqlite3_stmt pointer.
 *
 */
class StatementFinalizer {
private:
  sqlite3_stmt *stmt;

public:
  /**
   * @brief Construct a new Statement Finalizer object
   *
   * @param stmt A pointer to the statement for which this instance provides
   * finalization.
   */
  explicit StatementFinalizer(sqlite3_stmt *stmt) : stmt(stmt) {}

  // Prevent copying so it doesn't finalize twice
  StatementFinalizer(const StatementFinalizer &) = delete;
  StatementFinalizer &operator=(const StatementFinalizer &) = delete;

  /**
   * @brief Destroy the Statement Finalizer object (calls sqlite3_finalize).
   *
   */
  ~StatementFinalizer() {
    if (this->stmt != nullptr) {
      sqlite3_finalize(this->stmt);
      this->stmt = nullptr;
    }
  }
};

enum DraftSchemaViewVTabType {
  DRAFT_SCHEMA_VIEW_VTAB_INVALID = 0,
  DRAFT_SCHEMA_VIEW_VTAB_FILTER,
  DRAFT_SCHEMA_VIEW_VTAB_SHADOW
};

/**
 * @brief State object for a draft_schema_view vtable.
 *
 */
struct DraftSchemaViewVTab {
  sqlite3_vtab base;
  sqlite3 *db;
  DraftSchemaViewModuleState *pOwner;
  char *zVTableName; // Name of the virtual table
  char *zTargetName; // Name of the real backing table
  char *zShadowName; // Name of the shadow/index table maintaining lookup of
                     // relevant items
  char *
      zSchemaUri; // The URI of the schema for which this vtable provides a view
  DraftSchemaViewVTabType type;
  char *zKeyColumnList; // Comma-separated list of key columns
  int columnCount;
  char **zColumnList;
};

/**
 * @brief Specifies a cursor for the draft_schema_view module.
 *
 */
struct DraftSchemaViewCursor {
  sqlite3_vtab_cursor base;
  sqlite3_stmt *pStmt; /* Active query on the shadow table */
  int eof;
};

/**
 * @brief Holds a JSON draft schema and its associated validator within the
 * schema cache.
 *
 */
class CachedDraftSchema {

public:
  nlohmann::json schema;
  nlohmann::json_schema::json_validator validator;

  /**
   * @brief Construct a new Cached Draft Schema object with null schema and
   *        uninitialized validator.  Provided for compatibility with storage
   *        into std collections such as vector and map.
   *
   */
  CachedDraftSchema() {}

  /**
   * @brief Construct a new Cached Draft Schema object
   *
   * @param schema The schema for which to construct this instance.
   */
  explicit CachedDraftSchema(nlohmann::json schema) : schema(schema) {
    this->validator.set_root_schema(this->schema);
  }
};

/**
 * @brief State object for the draft_schama_view module.
 *
 */
class DraftSchemaViewModuleState {
private:
  std::map<std::string, CachedDraftSchema> cached_schemas;
  std::map<std::string, DraftSchemaViewVTab *> connected_vtables;
  std::set<DraftSchemaViewCursor *> open_cursors;
  const std::string schema_registry_table_name;
  const std::string view_registry_table_name;

public:
  /**
   * @brief Construct a new Draft Schema View Module State object
   *
   * @param view_registry_table_name The name for the draft_schema_view view
   * registry table.
   * @param schema_registry_table_name The name for the draft_schema_view schema
   * registry table.
   */
  DraftSchemaViewModuleState(const std::string_view view_registry_table_name,
                             const std::string_view schema_registry_table_name)
      : view_registry_table_name(view_registry_table_name),
        schema_registry_table_name(schema_registry_table_name) {}

  /**
   * @brief Gets the name for the draft_schema_view schema registry table.
   *
   * @return const std::string& The name for the draft_schema_view schema
   * registry table.
   */
  const std::string &getSchemaRegistryTableName() {
    return schema_registry_table_name;
  }

  /**
   * @brief Gets the name for the draft_schema_view view registry table.
   *
   * @return const std::string& The name for the draft_schema_view registry
   * table.
   */
  const std::string &getViewRegistryTableName() {
    return view_registry_table_name;
  }

  /**
   * @brief Retrieves and caches the schema associated with the specified URI
   * for the specified database.
   *
   * @param db A pointer to the database for which to get the schema.
   * @param schema_uri The URI of the schema to be retrieved.
   * @return CachedDraftSchema* A pointer to an an object holding the schema and
   * validator.
   */
  CachedDraftSchema *GetSchema(sqlite3 *db, std::string_view schema_uri) {

    std::string schema_uri_str(schema_uri);
    auto it = this->cached_schemas.find(schema_uri_str);
    if (it == this->cached_schemas.end()) {
      if (db != nullptr) {

        const char *schema_query_fmt = R"(
                    SELECT schema_json
                    FROM %s
                    WHERE schema_uri=:schema_uri
                )";

        auto schema_query = sqlite3_mprintf(
            schema_query_fmt, this->schema_registry_table_name.c_str());

        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, schema_query, -1, &stmt, nullptr);
        sqlite3_free(schema_query);
        if (rc != SQLITE_OK) {
          return nullptr;
        }
        StatementFinalizer defer_stmt_finalize(stmt);

        rc = sqlite3_bind_text(stmt, 1, schema_uri_str.c_str(), -1, nullptr);
        if (rc != SQLITE_OK) {
          auto err_msg = sqlite3_errmsg(db);
          std::cerr << err_msg;
          return nullptr;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
          return nullptr;
        }

        auto schema_len = sqlite3_column_bytes(stmt, 0);
        auto schema =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (schema == nullptr || schema_len == 0) {
          return nullptr;
        }

        auto schema_str = std::string(schema, schema + schema_len);
        this->cached_schemas.insert_or_assign(
            schema_uri_str,
            CachedDraftSchema(nlohmann::json::parse(schema_str)));

        it = this->cached_schemas.find(schema_uri_str);
        if (it == this->cached_schemas.end()) {
          return nullptr;
        }

        return &it->second;
      }

      return nullptr;
    }

    return &it->second;
  }

  /**
   * @brief Retrieves and caches the schema associated with the specified URI
   * for the specified context.
   *
   * @param ctx A pointer to the context for which to get the schema.
   * @param schema_uri The URI of the schema to be retrieved.
   * @return CachedDraftSchema* A pointer to an an object holding the schema and
   * validator.
   */
  CachedDraftSchema *GetSchema(sqlite3_context *ctx,
                               std::string_view schema_uri) {
    auto db = sqlite3_context_db_handle(ctx);
    return this->GetSchema(db, schema_uri);
  }

  /**
   * @brief Retrieves and caches the schema associated with the specified URI
   * within the context of the specified statement.
   *
   * @param stmt A pointer to the statement for which to get the schema.
   * @param schema_uri The URI of the schema to be retrieved.
   * @return CachedDraftSchema* A pointer to an an object holding the schema and
   * validator.
   */
  CachedDraftSchema *GetSchema(sqlite3_stmt *stmt,
                               std::string_view schema_uri) {
    auto db = sqlite3_db_handle(stmt);
    return this->GetSchema(db, schema_uri);
  }

  /**
   * @brief Registers a virtual table as connected.
   *
   * @param pVTab The pointer to the connected virtual table.
   */
  void RegisterConnectedVTable(DraftSchemaViewVTab *pVTab) {
    if (pVTab != nullptr && pVTab->pOwner == nullptr) {
      pVTab->pOwner = this;
      connected_vtables.insert_or_assign(std::string(pVTab->zVTableName),
                                         pVTab);
    }
  }

  /**
   * @brief Unregisters a connected virtual table.
   *
   * @param pVTab The pointer to the connected virtual table.
   */
  void UnregisterConnectedVTable(DraftSchemaViewVTab *pVTab) {
    if (pVTab != nullptr && pVTab->pOwner == this) {
      connected_vtables.erase(std::string(pVTab->zVTableName));
      pVTab->pOwner = nullptr;
    }
  }

  /**
   * @brief Gets a pointer to DraftSchemaViewVTab instance holding the
   *        state of the specified virtual table.
   *
   * @param vtable_name The name of the connected virtual table for which to get
   * the state.
   * @return DraftSchemaViewVTab* A pointer to the DraftSchemaViewVTab instance
   * (nullptr if not found).
   */
  DraftSchemaViewVTab *GetConnectedVTable(std::string_view vtable_name) {
    std::string vtable_name_str(vtable_name);
    auto found = this->connected_vtables.find(vtable_name_str);
    if (found == this->connected_vtables.end()) {
      return nullptr;
    }

    return (*found).second;
  }

  /**
   * @brief Gets a pointer to DraftSchemaViewVTab instance holding the
   *        state of the specified virtual table.
   *
   * @param vtable_name A pointer to the name of the connected virtual table for
   * which to get the state.
   * @return DraftSchemaViewVTab* A pointer to the DraftSchemaViewVTab instance
   * (nullptr if not found).
   */
  DraftSchemaViewVTab *GetConnectedVTable(const char *vtable_name) {
    if (vtable_name == nullptr) {
      return nullptr;
    }
    return this->GetConnectedVTable(std::string_view(vtable_name));
  }

  /**
   * @brief Registers the specified DraftSchemaViewCursor as being open.
   *
   * @param pCursor A pointer to the cursor to register.
   */
  void RegisterProxyCursor(DraftSchemaViewCursor *pCursor) {
    if (pCursor != nullptr) {
      open_cursors.insert(pCursor);
    }
  }

  /**
   * @brief Unregisters the specified DraftSchemaViewCursor (when closed).
   *
   * @param pCursor A pointer to the cursor to unregister.
   */
  void UnregisterProxyCursor(DraftSchemaViewCursor *pCursor) {
    if (pCursor != nullptr) {
      open_cursors.erase(pCursor);
    }
  }
};

/**
 * @brief Specifies the definition of a column from a table.
 *
 */
class ColumnDef {

private:
  const int ordinal;
  const std::string name;
  const std::string type;
  const bool not_null;
  const std::string dflt_value;
  const bool pk;

public:
  /**
   * @brief Construct a new Column Def object
   *
   * @param ordinal The ordinal of the column within the table.
   * @param name A pointer to the name of the column.
   * @param name_len The length of the name of the column.
   * @param type A pointer to the type for the column.
   * @param type_len The length of the type for the column.
   * @param not_null Indicates whether or not the column is required to not be
   * null.
   * @param dflt_value A pointer to the default value for the column.
   * @param dflt_value_len The length of the default value for the column.
   * @param pk Indicates whether or not this column is (part of) the primary
   * key.
   */
  ColumnDef(int ordinal, unsigned const char *name, int name_len,
            unsigned const char *type, int type_len, int not_null,
            unsigned const char *dflt_value, int dflt_value_len, bool pk)
      : ordinal(ordinal), name(read_string(name, name_len)),
        type(read_string(type, type_len)), not_null(not_null != 0),
        dflt_value(read_string(dflt_value, dflt_value_len)), pk(pk != 0) {}
  /**
   * @brief Construct a new Column Def object
   *
   * @param ordinal The ordinal of the column within the table.
   * @param name Specifies the name of the column.
   * @param type Specifies the type for the column.
   * @param not_null Indicates whether or not the column is required to not be
   * null.
   * @param dflt_value Specifies the default value for the column.
   * @param pk Indicates whether or not this column is (part of) the primary
   * key.
   */
  ColumnDef(int ordinal, std::string_view name, std::string_view type,
            bool not_null, std::string_view dflt_value, bool pk)
      : ordinal(ordinal), name(name), type(type), not_null(not_null),
        dflt_value(dflt_value), pk(pk) {}

  ColumnDef(const ColumnDef &) = default;

  /**
   * @brief Get the ordinal of the column within the table.
   *
   * @return const int The ordinal of the column within the table.
   */
  const int get_ordinal() const { return this->ordinal; }

  /**
   * @brief Get the name of the column.
   *
   * @return const std::string_view The name of the column.
   */
  const std::string_view get_name() const { return this->name; }

  /**
   * @brief Get the type of the column.
   *
   * @return const std::string_view The type of the column.
   */
  const std::string_view get_type() const { return this->type; }

  /**
   * @brief Gets a value indicating whether or not the column is required to not
   * be null.
   *
   * @return true The column is required to not be null.
   * @return false The column allows null.
   */
  bool is_not_null() const { return this->not_null; }

  /**
   * @brief Get the (string representation of the) default value of the column.
   *
   * @return const std::string_view The (string representation of) the default
   * value of the column.
   */
  const std::string_view get_dflt_value() const { return this->dflt_value; }

  /**
   * @brief Gets a value indicating whether or not this column is (a part of)
   * the primary key for the table.
   *
   * @return true This column is (a part of) the primary key for the table.
   * @return false This column is not a part of the primary key for the table.
   */
  const bool is_pk() const { return this->pk; }
};

/**
 * @brief Gets the names of the key columns for the specified vtable.
 *
 * @param pVTab A pointer to the virtual table for which to get the key columns.
 * @param key_columns A reference to the vector in which to store the key column
 * names.
 */
void draft_schema_view_vtab_get_key_columns(
    DraftSchemaViewVTab *pVTab, std::vector<std::string_view> &key_columns);

/**
 * @brief Frees the specified draft schema view vtable and its associated
 * resources.
 *
 * @param pVTab A pointer to the vtable to be freed.
 */
void draft_schema_view_vtab_free(DraftSchemaViewVTab *pVTab);
