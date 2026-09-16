#pragma once

#include "dsv_common.h"
#include "sqlite3.h"

// **** SQL Module callback function declarations ****

/**
 * @brief Implementation of the sqlite3_module xCreate callback for
 * draft_schema_viewer. Creates a draft_schema_view virtual table view over a
 * specified target table.
 *
 * @param db A pointer to the database in which to create the virtual table.
 * @param pAux A pointer to the DraftSchemaViewModuleState for which to create
 * the virtual table.
 * @param argc Specifies the number of SQL arguments provided for the call.
 * @param argv A pointer to the array of SQL arguments for the call.
 * @param ppVTab Receives a pointer to the virtual table created.
 * @param pzErr Receives a pointer to the error message, if any, upon failure.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_create(sqlite3 *db, void *pAux, int argc,
                             const char *const *argv, sqlite3_vtab **ppVTab,
                             char **pzErr);

/**
 * @brief Implementation of the sqlite3_module xConnect callback for
 * draft_schema_viewer. Connects a draft_schema_viewer virtual table.
 *
 * @param db A pointer to the db for which to connect the virtual table.
 * @param pAux A pointer to the DraftSchemaViewModuleState for which to create
 * the virtual table.
 * @param argc Specifies the number of SQL arguments provided for the call.
 * @param argv A pointer to the array of SQL arguments for the call.
 * @param ppVTab Receives a pointer to the virtual table created.
 * @param pzErr Receives a pointer to the error message, if any, upon failure.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_connect(sqlite3 *db, void *pAux, int argc,
                              const char *const *argv, sqlite3_vtab **ppVTab,
                              char **pzErr);

/**
 * @brief Implementation of the sqlite3_module xBestIndex callback for
 * draft_schema_view. Provides information about the best index for queries of
 * the virtual table.
 *
 * @param pVTab A pointer to the virtual table for which to get index
 * information.
 * @param pIdxInfo Receives the information about the index.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_best_index(sqlite3_vtab *pVTab,
                                 sqlite3_index_info *pIdxInfo);

/**
 * @brief Implementation of the sqlite3_module xDisconnect callback for
 * draft_schema_view. Disconnects a connected virtual table.
 *
 * @param pVTab A pointer to the virtual table to be disconnected.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_disconnect(sqlite3_vtab *pVTab);

/**
 * @brief Implementation of the sqlite3_module xDestroy callback for
 * draft_schema_view. Destroys the specified connected virtual table.
 *
 * @param pVTab A pointer to the virtual table to be destroyed.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_destroy(sqlite3_vtab *pVTab);

/**
 * @brief Implementation of the sqlite3_module xUpdate callback for
 * draft_schema_view. Handles insertions and updates to the virtual table.
 *
 * @param pVTab A pointer to the virtual table for which the insertion or update
 * was made.
 * @param argc Specifies the number of SQL arguments provided for the call.
 * @param argv A pointer the SQL arguments provided for the call.
 * @param pRowid On insertion, receives the rowid of the inserted row.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_update(sqlite3_vtab *pVTab, int argc,
                             sqlite3_value **argv, sqlite3_int64 *pRowid);

/**
 * @brief Opens a cursor for a virtual table of the draft_schema_view module.
 *
 * @param pVTab A pointer to the draft_schema_view virtual table for which to
 * open the cursor.
 * @param ppCursor Receives the pointer to the opened cursor.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_cursor_open(sqlite3_vtab *pVTab,
                                  sqlite3_vtab_cursor **ppCursor);

/**
 * @brief Implementation of the sqlite3_module xCursorClose callback for
 * draft_schema_view. Closes a cursor for a virtual table of the
 * draft_schema_view module.
 *
 * @param pCursor A pointer to the cursor to be closed.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_cursor_close(sqlite3_vtab_cursor *pCursor);

/**
 * @brief Implementation of the sqlite3_module xFilter callback for
 * draft_schema_view. Initializes the filter for a virtual table query.
 *
 * @param pCursor A pointer to the cursor for which to filter the virtual table.
 * @param idxNum Ignored
 * @param idxStr Ignored
 * @param argc Ignored
 * @param argv Ignored
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_cursor_filter(sqlite3_vtab_cursor *pCursor, int idxNum,
                                    const char *idxStr, int argc,
                                    sqlite3_value **argv);

/**
 * @brief Implementation of the sqlite3_module xCursorNext.
 * Advances the specified draft_schema_view vtable cursor to the next result
 * row.
 *
 * @param pCursor A pointer to the cursor to be advanced.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_cursor_next(sqlite3_vtab_cursor *pCursor);

/**
 * @brief Implementation of the sqlite3_module xCursorEof callback.
 * Gets a value indicating whether or not the end of the results have been
 * reached.
 *
 * @param pCursor A pointer for the cursor for which to check for eof.
 * @return int 0 if not eof; 1 if eof.
 */
int draft_schema_view_cursor_eof(sqlite3_vtab_cursor *pCursor);

/**
 * @brief Implementation of the sqlite3_module xCursorColumn callback for
 * draft_schema_view. Gets the value of the specified column for the current
 * cursor position.
 *
 * @param pCursor A pointer to the cursor for which to get the column value.
 * @param ctx A pointer to the context of the query.
 * @param iCol The index of the column for which to get the value.
 * @return int An SQLITE_ constant indicating success or failure.
 */
int draft_schema_view_cursor_column(sqlite3_vtab_cursor *pCursor,
                                    sqlite3_context *ctx, int iCol);

/**
 * @brief Implementation of the sqlite3_module xCursorRowid callback for
 * draft_schema_view. Retrieves the rowid for the current result row of the
 * specified draft_schema_view cursor.
 *
 * @param pCursor A pointer to the cursor for which to get the rowid.
 * @param pRowid Receives the rowid.
 * @return int An SQLITE_ constant specifying success or failure.
 */
int draft_schema_view_cursor_rowid(sqlite3_vtab_cursor *pCursor,
                                   sqlite3_int64 *pRowid);

/**
 * @brief Implementation for the sqlite3_module's client data destroy callback.
 *
 * @param pClientData A pointer to the DraftSchemaViewModuleState to be
 * destroyed.
 */
void draft_schema_view_module_destroy(void *pClientData);
