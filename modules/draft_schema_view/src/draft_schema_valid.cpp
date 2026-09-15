#include "draft_schema_valid.h"
#include "dsv_common.h"
#include "dsv_sql_helpers.h"
#include "sqlite3.h"

void draft_schema_valid(sqlite3_context *ctx, int argc, sqlite3_value **argv) {

  if (argc < 3 || (argc % 2 != 1)) {
    sqlite3_result_error(
        ctx, "Usage: draft_schema_valid(schema_uri, colname1, colvalue1, ...)",
        -1);
    return;
  }

  // Retrieve the schema registry pointer passed at registration
  auto *schema_viewer_state =
      static_cast<DraftSchemaViewModuleState *>(sqlite3_user_data(ctx));
  if (schema_viewer_state == nullptr) {
    sqlite3_result_error(ctx, "Failed to obtain draft schema viewer state.",
                         -1);
    return;
  }

  // Lookup the cached schema by URI
  auto schema_uri_length = sqlite3_value_bytes(argv[0]);
  auto schema_uri = reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
  if (schema_uri == nullptr || schema_uri_length == 0) {
    sqlite3_result_error(ctx, "schema_uri cannot be null or empty.", -1);
    return;
  }

  // argv[1] through argv[argc - 1] contain the column names and raw column
  // values
  int colc = (argc - 1) / 2;
  sqlite3_value **colv = &argv[1];

  try {
    std::string_view uri_str =
        std::string_view(schema_uri, schema_uri + schema_uri_length);
    auto cached_schema = schema_viewer_state->GetSchema(ctx, uri_str);
    if (cached_schema == nullptr) {
      sqlite3_result_error(ctx, "The specified schema is not registered.", -1);
      return;
    }

    nlohmann::json row_obj =
        construct_json_object(cached_schema->schema, colc, colv);

    // Validate and yield boolean
    try {
      cached_schema->validator.validate(row_obj);
      sqlite3_result_int(ctx, 1);
    } catch (...) {
      sqlite3_result_int(ctx, 0);
    }
  } catch (std::exception &e) {
    sqlite3_result_error(ctx, e.what(), -1);
    return;
  } catch (...) {
    sqlite3_result_error(
        ctx, "An unknown error occurred validating the schema.", -1);
  }
}

int draft_schema_valid_func_register(sqlite3 *db,
                                     DraftSchemaViewModuleState *pViewerState) {
  return sqlite3_create_function_v2(
      db, "draft_schema_valid",
      -1, // Accepts any number of columns dynamically
      SQLITE_UTF8 | SQLITE_DETERMINISTIC,
      pViewerState, // Passed into sqlite3_user_data(ctx)
      draft_schema_valid, nullptr, nullptr, nullptr);
}
