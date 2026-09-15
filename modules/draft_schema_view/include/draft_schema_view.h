#pragma once

#include "sqlite3.h"

#define DEFAULT_DRAFT_SCHEMA_VIEW_REGISTRY_NAME "draft_schema_view_registry"
#define DEFAULT_DRAFT_SCHEMA_VIEW_SCHEMA_REGISTRY_TABLE_NAME                   \
  "draft_schema_view_schema_registry"

/**
 * @brief Initializes the draft_schema_view module for the specified database.
 *
 * @param db A pointer to the database for which to register the
 * draft_schema_view module.
 * @param view_registry_table_name A pointer to the name for the vtable view
 * registry table.
 * @param schema_registry_table_name A pointer to the name for the schema
 * registry table.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_init(
    sqlite3 *db,
    const char *view_registry_table_name =
        DEFAULT_DRAFT_SCHEMA_VIEW_REGISTRY_NAME,
    const char *schema_registry_table_name =
        DEFAULT_DRAFT_SCHEMA_VIEW_SCHEMA_REGISTRY_TABLE_NAME);
