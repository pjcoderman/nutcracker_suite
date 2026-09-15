#pragma once

#include "dsv_common.h"
#include "nlohmann/json.hpp"
#include "sqlite3.h"


#include <string>
#include <vector>

// SQL table schema helpers

/**
 * @brief Gets the SQL definition of the specified table.
 *
 * @param db A pointer to the database from which to get the table SQL.
 * @param table_name The name of the table for which to get the table SQL.
 * @param table_sql Receives the table SQL definition.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int sqlite3_get_table_sql(sqlite3 *db, std::string_view table_name,
                          std::string &table_sql);

/**
 * @brief Gets the definitions of the columns for the specified table.
 *
 * @param db A pointer to the database from which to get the column definitions
 * of the table.
 * @param table_name The name of the table for which to get the column
 * definitions.
 * @param column_defs The vector in which to store the column definitions.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int get_column_defs(sqlite3 *db, const std::string_view table_name,
                    std::vector<ColumnDef> &column_defs);

/**
 * @brief Gets the sets of names of columns making up unique constraints for the
 * specified table.
 *
 * @param db A pointer to the database for which to get the unique constraints.
 * @param table_name The name of the table for which to get the unique
 * constraints.
 * @param unique_constraints Receives a vector of the column names making up the
 * column names of each unique constraint for each unique constraint of the
 * specified table.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int get_unique_constraints(
    sqlite3 *db, const std::string_view table_name,
    std::vector<std::vector<std::string>> &unique_constraints);

// SQL JSON helpers

/**
 * @brief Binds the values of the specified statement, beginning at the
 * specified ordinal, to the fields of the specified JSON given the definitions
 * of the columns of the table.
 *
 * @param stmt A pointer to the statement for which to bind the values.
 * @param ord The ordinal at which to begin binding JSON fields.
 * @param row_json The JSON object specifying the field/column values to be
 * bound.
 * @param column_defs A vector of the column definitions for the table.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int sqlite3_bind_json(sqlite3_stmt *stmt, int ord, nlohmann::json &row_json,
                      std::vector<ColumnDef> &column_defs);

/**
 * @brief Converts the specified SQL value to an nlohmann::json value.
 *
 * @param value A pointer to the SQL value to be converted.
 * @param prop_schema Specifies the schema fragment corresponding to the
 * property (not enforced here; used for type hints).
 * @return nlohmann::json An nlohmann::json value representing the specified SQL
 * value.
 */
[[nodiscard]] nlohmann::json sql_value_to_json(sqlite3_value *value,
                                               nlohmann::json &prop_schema);

/**
 * @brief Constructs a JSON object from an interlaced array of SQL values
 * containing the column names and values.
 *
 * @param schema Specifies the schema to which the object is intended to comply
 * (not enforced here; used for type hints).
 * @param colc Specifies the number of columns for which the name and value are
 * present.
 * @param col_names_values A pointer to an array of column names and values
 * interlaced as col_name, col_value, ...
 * @return nlohmann::json An nlohmann::json value comprised of the specified
 * values typed as secified by schema where possible.
 */
[[nodiscard]] nlohmann::json
construct_json_object(nlohmann::json &schema, int colc,
                      sqlite3_value **col_names_values);

/**
 * @brief Constructs a JSON row object for the specified column values IAW the
 * specified shema.
 *
 * @param pVTab A pointer to the vtable for which to construct the JSON row
 * object.
 * @param colc The number of column values for which to construct the JSON row
 * object.
 * @param colv A pointer to an array of column values for which to construct the
 * JSON row object.
 * @return nlohmann::json A JSON row object containing the specified column
 * values IAW the specified schema.
 */
[[nodiscard]] nlohmann::json construct_json_row(DraftSchemaViewVTab *pVTab,
                                                int colc, sqlite3_value **colv);
