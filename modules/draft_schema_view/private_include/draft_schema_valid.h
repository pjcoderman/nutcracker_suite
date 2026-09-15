#pragma once

#include "draft_schema_view.h"
#include "dsv_common.h"

#include "sqlite3.h"

/**
 * @brief Registers the draft_schema_valid function with the specified database
 *        on behalf of the draft_schema_view module.
 *
 * @param db The sqlite3 database for which to register the function.
 * @param pViewerState A pointer to the state of the draft_schema_view module.
 * @return int The SQLITE_ constant specifying success or failure of the call.
 */
int draft_schema_valid_func_register(sqlite3 *db,
                                     DraftSchemaViewModuleState *pViewerState);

/**
 * @brief Implementation callback for the draft_schema_valid function.
 *
 * @param ctx The SQL context for which the function was called.
 * @param argc The number of arguments specified for the function call.
 * @param argv A pointer to an array of the arguments specified in the function
 * call.
 */
void draft_schema_valid(sqlite3_context *ctx, int argc, sqlite3_value **argv);
