
#pragma once

#include "dsv_common.h"
#include <vector>

/**
 * @brief Creates the insert valid trigger for the specified vtable, ensuring
 * that when a row of the target table is valid IAW the registered schema, it
 * becomes registered in the shadow table.
 *
 * @param pVTab A pointer to the vtable for which to create the trigger.
 * @param key_columns Specifies the names of the key columns for the vtable.
 * @param column_defs Specifies the definitions of the target table columns.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_create_insert_valid_trigger(
    DraftSchemaViewVTab *pVTab,
    const std::vector<std::string_view> &key_columns,
    std::vector<ColumnDef> &column_defs);

/**
 * @brief Creates the update valid trigger for the specified vtable, ensuring
 * that when a row of the target table is valid IAW the registered schema, it
 * becomes registered or updated in the shadow table.
 *
 * @param pVTab A pointer to the vtable for which to create the trigger.
 * @param key_columns Specifies the names of the key columns for the vtable.
 * @param column_defs Specifies the definitions of the target table columns.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_create_update_valid_trigger(
    DraftSchemaViewVTab *pVTab,
    const std::vector<std::string_view> &key_columns,
    std::vector<ColumnDef> &column_defs);

/**
 * @brief Creates the update invalid trigger for the specified vtable, ensuring
 * that when a row of the target table is invalid IAW the registered schema, it
 * is removed from the shadow table.
 *
 * @param pVTab A pointer to the vtable for which to create the trigger.
 * @param key_columns Specifies the names of the key columns for the vtable.
 * @param column_defs Specifies the definitions of the target table columns.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_create_update_invalid_trigger(
    DraftSchemaViewVTab *pVTab,
    const std::vector<std::string_view> &key_columns,
    std::vector<ColumnDef> &column_defs);

/**
 * @brief Creates the required triggers for the specified vtable, ensuring that
 * valid rows IAW the registered schema are maintained within the shadow table.
 *
 * @param pVTab A pointer to the vtable for which to create the triggers.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_create_triggers(DraftSchemaViewVTab *pVTab);
