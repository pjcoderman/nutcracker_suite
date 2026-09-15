#pragma once

#include "dsv_common.h"

#include "nlohmann/json.hpp"
#include "sqlite3.h"


/**
 * @brief Creates or updates the draft_schema_view JSON draft schema registry
 * for the specified database.
 *
 * @param db A pointer to the database for which to create the schema registry.
 * @param schema_registry_table_name A pointer to the name for the draft schema
 * registry table.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_schema_registry_create_or_update(
    sqlite3 *db, const char *schema_registry_table_name);

/**
 * @brief Creates or updates the draft_schema_view vtable registry table for the
 * specified database.
 *
 * @param db A pointer to the database for which to create or register the
 * vtable registry.
 * @param view_registry_table_name A pointer to the name for the vtable registry
 * table.
 * @param schema_registry_table_name A pointer to the name for the schema
 * registry table.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_vtable_registry_create_or_update(
    sqlite3 *db, const char *view_registry_table_name,
    const char *schema_registry_table_name);

/**
 * @brief Inserts a schema into the draft_schema_view schema registry table for
 * the specified database.
 *
 * @param db A pointer to the database for which to insert the schema.
 * @param schema_registry_table_name A pointer to the name of the schema
 * registry table.
 * @param schema_uri A pointer to the URI of the schema.
 * @param schema_json Specifies the schema to be stored.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_insert_into_schema_registry(
    sqlite3 *db, const char *schema_registry_table_name, const char *schema_uri,
    nlohmann::json &schema_json);

/**
 * @brief Inserts a vtable view registration into the vtable registry table for
 * the specified database.
 *
 * @param db A pointer to the database for which to register the vtable.
 * @param view_registry_table_name A pointer to the name of the vtable registry
 * table.
 * @param vtable_name A pointer to the name of the vtable to register.
 * @param type Specifies the type of view to create.
 * @param table_name A pointer to the name of the target table for which the
 * vtable provides a view.
 * @param schema_uri A pointer to the URI of the schema for the vtable view.
 * @param key_cols_str A pointer to a comma-separated list of the key columns
 * from the target table uniquely identifying a row for the view.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_insert_into_view_registry(
    sqlite3 *db, const char *view_registry_table_name, const char *vtable_name,
    const DraftSchemaViewVTabType type, const char *table_name,
    const char *schema_uri, const char *key_cols_str);

/**
 * @brief Removes a vtable view registration from the vtable registry table for
 * the specified database.
 *
 * @param db A pointer to the database for which remove the vtable registration.
 * @param view_registry_table_name A pointer to the name of the vtable registry
 * table.
 * @param vtable_name A pointer to the vtable name for which to remove the
 * registration.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_viewer_remove_from_view_registry(
    sqlite3 *db, const char *view_registry_table_name, const char *vtable_name);
