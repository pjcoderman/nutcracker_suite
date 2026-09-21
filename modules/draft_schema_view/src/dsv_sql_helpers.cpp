#include "dsv_sql_helpers.h"
#include "base64.h"

#include <iostream>

int sqlite3_get_table_sql(sqlite3 *db, std::string_view table_name,
                          std::string &table_sql) {

  if (db == nullptr || table_name.empty()) {
    return SQLITE_INTERNAL;
  }

  const std::string query_sql = R"(
        SELECT sql, name
        FROM sqlite_schema 
        WHERE sql IS NOT NULL 
            AND type='table' 
            AND name=:table_name;    
    )";

  sqlite3_stmt *query_stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, query_sql.c_str(), -1, &query_stmt, NULL);
  if (rc != SQLITE_OK) {
    return rc;
  }
  StatementFinalizer defer_stmt_finalize(query_stmt);

  std::string table_name_str(table_name);
  rc = sqlite3_bind_text(query_stmt, 1, table_name_str.c_str(), -1,
                         SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_step(query_stmt);
  if (rc == SQLITE_ROW) {

    // Read table sql from result
    auto table_sql_length = sqlite3_column_bytes(query_stmt, 0);
    auto *pSql =
        reinterpret_cast<const char *>(sqlite3_column_text(query_stmt, 0));
    table_sql = read_string(pSql, table_sql_length);
    if (table_sql.length() == 0) {
      return SQLITE_ERROR;
    }

    auto name_length = sqlite3_column_bytes(query_stmt, 1);
    auto pName =
        reinterpret_cast<const char *>(sqlite3_column_text(query_stmt, 1));
    auto name = read_string(pName, name_length);
    return SQLITE_OK;
  }

  if (rc == SQLITE_DONE) {
    return SQLITE_NOTFOUND;
  }

  return rc;
}

int get_column_defs(sqlite3 *db, const std::string_view table_name,
                    std::vector<ColumnDef> &column_defs) {

  auto pragma_table_info_qry =
      std::format("PRAGMA table_info('{}');", table_name);

  // Prepare the statement
  sqlite3_stmt *stmt = nullptr;
  int rc =
      sqlite3_prepare_v2(db, pragma_table_info_qry.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }
  StatementFinalizer defer_finalize_col_query(stmt);

  int ord = 1;
  while (rc != SQLITE_DONE) {
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
      auto cid = sqlite3_column_int(stmt, 0);
      auto name_len = sqlite3_column_bytes(stmt, 1);
      auto pName = sqlite3_column_text(stmt, 1);
      auto type_len = sqlite3_column_bytes(stmt, 2);
      auto pType = sqlite3_column_text(stmt, 2);
      auto notnull = sqlite3_column_int(stmt, 3);
      auto dflt_value_len = sqlite3_column_bytes(stmt, 4);
      auto pDefaultValue = sqlite3_column_text(stmt, 4);
      auto pk = sqlite3_column_int(stmt, 5);

      column_defs.push_back(ColumnDef(
          cid, read_string(pName, name_len), read_string(pType, type_len),
          notnull == 1, read_string(pDefaultValue, dflt_value_len), pk == 1));
    } else if (rc != SQLITE_DONE) {
      return SQLITE_ERROR;
    }
  }

  return SQLITE_OK;
}

// Builds a vector of vectors containing the column names for each unique
// constraint
int get_unique_constraints(
    sqlite3 *db, const std::string_view table_name,
    std::vector<std::vector<std::string>> &unique_constraints) {

  // Step 1: Query all indexes matching the table
  // Column 1 ("name") contains index names
  // Column 2 ("unique") indicates if it's UNIQUE (1) or not (0)
  // const char* list_query = "PRAGMA index_list(:table_name);";
  const std::string index_list_qry =
      std::format("PRAGMA index_list('{}');", table_name);

  sqlite3_stmt *list_stmt = nullptr;
  int rc =
      sqlite3_prepare_v2(db, index_list_qry.c_str(), -1, &list_stmt, nullptr);
  if (rc != SQLITE_OK) {
    return SQLITE_ERROR;
  }

  StatementFinalizer defered_stmt_finalize(list_stmt);

  // Track unique index names
  std::vector<std::string> unique_index_names;
  while ((rc = sqlite3_step(list_stmt)) == SQLITE_ROW) {
    int is_unique =
        sqlite3_column_int(list_stmt, 2); // 'unique' flag is at index 2
    if (is_unique == 1) {
      const unsigned char *idx_name_raw =
          sqlite3_column_text(list_stmt, 1); // 'name' is at index 1
      if (idx_name_raw) {
        unique_index_names.emplace_back(
            reinterpret_cast<const char *>(idx_name_raw));
      }
    }
  }

  if (rc != SQLITE_DONE) {
    return SQLITE_ERROR;
  }

  // Step 2: Iterate over each unique index to fetch its target columns
  for (const auto &index_name : unique_index_names) {
    const char *info_query = "PRAGMA index_info(:index_name);";
    sqlite3_stmt *info_stmt = nullptr;
    rc = sqlite3_prepare_v2(db, info_query, -1, &info_stmt, nullptr);
    if (rc != SQLITE_OK) {
      return SQLITE_ERROR;
    }

    StatementFinalizer deferred_info_finalize(info_stmt);
    rc = sqlite3_bind_text(info_stmt, 1, index_name.data(), -1, nullptr);
    if (rc != SQLITE_OK) {
      return SQLITE_ERROR;
    }

    std::vector<std::string> constraint_columns;
    while ((rc = sqlite3_step(info_stmt)) == SQLITE_ROW) {
      const unsigned char *col_name_raw =
          sqlite3_column_text(info_stmt, 2); // 'name' of column is at index 2
      if (col_name_raw) {
        constraint_columns.emplace_back(
            reinterpret_cast<const char *>(col_name_raw));
      }
    }

    if (rc != SQLITE_DONE) {
      return SQLITE_ERROR;
    }

    if (!constraint_columns.empty()) {
      unique_constraints.push_back(std::move(constraint_columns));
    }
  }

  return SQLITE_OK;
}

[[nodiscard]] nlohmann::json sql_value_to_json(sqlite3_value *value,
                                               nlohmann::json &prop_schema) {
  bool supports_object = false, supports_string = false, supports_array = false;
  std::string contentEncoding, strFormat;
  std::string prop_type;
  nlohmann::json type_json;

  switch (sqlite3_value_type(value)) {

  case SQLITE_NULL:
    return nlohmann::json{};

  case SQLITE_INTEGER: {
    bool is_boolean = false;
    if (prop_schema.is_string()) {
      prop_type = prop_schema.get<std::string>();
      is_boolean = prop_type == "boolean";
    } else if (prop_schema.is_object()) {
      type_json = prop_schema.value("type", nlohmann::json{});
      if (type_json.is_string()) {
        prop_type = type_json.get<std::string>();
        is_boolean = prop_type == "boolean";
      } else if (type_json.is_array()) {
        for (auto it = type_json.begin(); it != type_json.end(); ++it) {
          prop_type = it->get<std::string>();
          is_boolean = prop_type == "boolean";
          if (is_boolean) {
            break;
          }
        }
      }
    }

    auto int_value = sqlite3_value_int64(value);
    if (is_boolean && (int_value == 0 || int_value == 1)) {
      return nlohmann::json(int_value == 1);
    }

    return nlohmann::json(int_value);
  } break;

  case SQLITE_FLOAT:
    return nlohmann::json(sqlite3_value_double(value));

  case SQLITE_BLOB: {
    auto blob_len = sqlite3_value_bytes(value);
    auto blob = (const uint8_t *)sqlite3_value_blob(value);

    std::vector<uint8_t> blob_vec;
    if ((blob != nullptr) && (blob_len > 0)) {
      blob_vec = std::vector<uint8_t>(blob, blob + blob_len);
    }

    if (prop_schema.is_string()) {
      prop_type = prop_schema.get<std::string>();
      supports_object = prop_type == "object";
      supports_string = prop_type == "string";
      supports_array = prop_type == "array";
    } else if (prop_schema.is_object()) {
      type_json = prop_schema.value("type", nlohmann::json{});
      if (type_json.is_string()) {
        prop_type = type_json.get<std::string>();
        supports_object = prop_type == "object";
        supports_string = prop_type == "string";
        supports_array = prop_type == "array";
      } else if (type_json.is_array()) {
        for (auto it = type_json.begin(); it != type_json.end(); ++it) {
          prop_type = it->get<std::string>();
          if (prop_type == "object") {
            supports_object = true;
          } else if (prop_type == "string") {
            supports_string = true;
          } else if (prop_type == "array") {
            supports_array = true;
          }
          if (supports_object && supports_string && supports_array) {
            break;
          }
        }
      }

      auto enc = prop_schema.value("contentEncoding", nlohmann::json{});
      if (enc.is_string()) {
        contentEncoding = enc.get<std::string>();
      }

      auto fmt = prop_schema.value("format", nlohmann::json{});
      if (fmt.is_string()) {
        strFormat = fmt.get<std::string>();
      }
    }

    if (supports_string) {
      return nlohmann::json(base64_encode(blob, blob_len));
    } else if (supports_object || supports_array) {
      auto col_json = nlohmann::json::from_bson(blob_vec);
      if (supports_object && col_json.is_object()) {
        return nlohmann::json::from_bson(blob_vec);
      } else if (supports_array && col_json.is_array()) {
        return nlohmann::json::from_bson(blob_vec);
      } else {
        return nlohmann::json::binary(blob_vec);
      }
    } else {
      return nlohmann::json::binary(blob_vec);
    }
  } break;
  case SQLITE_TEXT: {

    auto text_len = sqlite3_value_bytes(value);
    auto text = sqlite3_value_text(value);

    std::string text_str;
    if ((text != nullptr) && (text_len > 0)) {
      text_str = std::string(text, text + text_len);
    }

    if (prop_schema.is_string()) {
      prop_type = prop_schema.get<std::string>();
      supports_object = prop_type == "object";
      supports_string = prop_type == "string";
      supports_array = prop_type == "array";
    } else if (prop_schema.is_object()) {
      type_json = prop_schema.value("type", nlohmann::json{});
      if (type_json.is_string()) {
        prop_type = type_json.get<std::string>();
        supports_object = prop_type == "object";
        supports_string = prop_type == "string";
        supports_array = prop_type == "array";
      } else if (type_json.is_array()) {
        for (auto it = type_json.begin(); it != type_json.end(); ++it) {
          prop_type = it->get<std::string>();
          if (prop_type == "object") {
            supports_object = true;
          } else if (prop_type == "string") {
            supports_string = true;
          } else if (prop_type == "array") {
            supports_array = true;
          }
          if (supports_object && supports_string && supports_array) {
            break;
          }
        }
      }
    }

    if ((supports_object || supports_array) && !supports_string) {
      return nlohmann::json::parse(text_str);
    } else {
      return nlohmann::json(text_str);
    }
  } break;
  default:
    throw std::runtime_error("Invalid SQL value type");
  }
}

[[nodiscard]] nlohmann::json
construct_json_object(nlohmann::json &schema, int colc,
                      sqlite3_value **col_names_values) {

  nlohmann::json row_json = nlohmann::json::object();

  nlohmann::json properties;
  if (schema.is_object()) {
    properties = schema.value("properties", nlohmann::json{});
  }
  int valc = colc * 2;

  for (auto i = 0; i < valc; i += 2) {
    auto col_name_val = col_names_values[i];
    auto col_name_len = sqlite3_value_bytes(col_name_val);
    auto col_name_chars = sqlite3_value_text(col_name_val);

    auto col_val = col_names_values[i + 1];
    auto col_name = read_string(col_name_chars, col_name_len);

    nlohmann::json prop_schema;
    if (properties.is_object()) {
      prop_schema = properties.value(col_name, nlohmann::json{});
    }
    row_json[col_name] = sql_value_to_json(col_val, prop_schema);
  }

  return row_json;
}

[[nodiscard]] nlohmann::json
construct_json_row(DraftSchemaViewVTab *pVTab, int colc, sqlite3_value **colv) {
  if (colc != pVTab->columnCount) {
    throw std::runtime_error("Column count does not match that of the vtable.");
  }

  nlohmann::json row_json = nlohmann::json::object();

  auto pSchema =
      pVTab->pOwner->GetSchema(pVTab->db, std::string_view(pVTab->zSchemaUri));

  nlohmann::json properties = nlohmann::json{};

  if (pSchema != nullptr && pSchema->schema.is_object()) {
    properties = pSchema->schema.value("properties", nlohmann::json{});
  }

  for (auto i = 0; i < colc; i++) {
    auto col_val = colv[i];
    auto col_name = pVTab->zColumnList[i];

    if (col_val == nullptr || col_name == nullptr) {
      throw std::runtime_error("A provided column name or value was null.");
    }

    auto prop_schema = properties.value(col_name, nlohmann::json{});
    row_json[col_name] = sql_value_to_json(col_val, prop_schema);
  }

  return row_json;
}
