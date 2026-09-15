#include "semantic_database.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "nlohmann/json-schema.hpp"
#include "sqlite-vec.h"
#include "sqlite3.h"

using nlohmann::json;
using nlohmann::json_schema::json_validator;

#define INCLUDE_USERS_EXAMPLE 1

namespace semantic_database {

    #if INCLUDE_USERS_EXAMPLE

    const std::string EXAMPLE_User_Schema = R"(
        {
            "$schema": "http://json-schema.org/draft-07/schema#",
            "$id": "https://example.com/schemas/user.schema.json",
            "title": "User",
            "description": "Schema representing a record from the users table",
            "type": "object",
            "properties": {
                "id": {
                    "type": "integer",
                    "description": "Primary key identifier for the user"
                },
                "name": {
                    "type": "string",
                    "description": "The user's name",
                    "not": {
                        "const": "Alice"
                    }
                },
                "details": {
                    "type": "object",
                    "additionalProperties": true
                }
            },
            "required": [
                "id"
            ],
            "additionalProperties": false
        }
    )";

    #endif // Users example schema
    
    const std::string CREATE_TABLE_json_schema_store = R"(
        CREATE TABLE json_schema_store (
            id          INTEGER PRIMARY KEY,
            schema_uri  TEXT GENERATED ALWAYS AS (
                json_extract(schema_json, '$.$id')
            ) STORED UNIQUE,
            schema_json TEXT NOT NULL CHECK (
                json_valid(schema_json)
            )
        );
    )";

    const std::string INSERT_INTO_json_schema_store = R"(
        INSERT INTO json_schema_store (schema_json) 
        VALUES (:schema_json);
    )";

    const std::string SELECT_json_schema_by_uri = R"(
        SELECT schema_json
        FROM json_schema_store
        WHERE schema_uri = :schema_uri;
    )";

    const std::string CREATE_TABLE_table_json_schema = R"(
        CREATE TABLE table_json_schema (
           table_name  TEXT NOT NULL PRIMARY KEY,
           schema_uri  TEXT NOT NULL REFERENCES json_schema_store(schema_uri)
                ON UPDATE CASCADE ON DELETE RESTRICT
        );
    )";

    const std::string INSERT_INTO_table_json_schema = R"(
        INSERT INTO table_json_schema (table_name, schema_uri)
        VALUES (:table_name, :schema_uri);
    )";

    const std::string SELECT_table_schema_uri_by_table_name = R"(
        SELECT schema_uri
        FROM table_json_schema
        WHERE table_name = :table_name;
    )";

    const std::string SELECT_table_schema_by_table_name = R"(
        SELECT ss.schema_json 
        FROM json_schema_store AS ss 
        INNER JOIN table_json_schema AS ts 
            ON ts.schema_uri = ss.schema_uri 
        WHERE ts.table_name = :table_name;
    )";

    inline json parse_json(std::string_view schema_str) {
        if (schema_str.length() == 0) {
            return nullptr;
        }
        try {
            return json::parse(schema_str.begin(), schema_str.end());
        }
        catch(std::invalid_argument& e) {
            throw SQLite::Exception("Invalid JSON string: " + std::string(e.what()));
        }
    }

    void validate_schema(json schema_json) {
        
        if (schema_json == nullptr) {
            throw SQLite::Exception("JSON schema cannot be NULL or empty");
        }

        if (!schema_json.is_object()) {
            throw SQLite::Exception("JSON schema must be an object");
        }

        auto id = schema_json["$id"];
        if (id == nullptr || !id.is_string()) {
            throw SQLite::Exception("JSON schema must specify a non-blank $id");
        }
    }

    json load_draft_schema(const std::filesystem::path& schema_path) {
        std::ifstream file(schema_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open schema file: " + schema_path.string());
        }
        auto schema = json::parse(file);
        validate_schema(schema);
        return schema;
    }

    class TableColumn {
        public:
        int ordinal;
        int hidden;
        std::string name;
        TableColumn(int ordinal, int hidden, std::string name):ordinal(ordinal), hidden(hidden), name(name) { }
    };

    std::vector<TableColumn> get_table_columns(sqlite3* db, std::string table_name) {
        std::vector<TableColumn> cols;
        sqlite3_stmt* stmt = nullptr;

        // hidden = 0 filters out generated/virtual columns (hidden=2) and system rowid pseudo-columns (hidden=1)
        const char* sql = "SELECT name, hidden FROM pragma_table_xinfo(?1) ORDER BY cid;";
        int ordinal = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, table_name.data(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto column_name_length = sqlite3_column_bytes(stmt, 0);
                auto column_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                auto hidden = sqlite3_column_int(stmt, 1);
                std::string col_name_str;
                if (column_name && column_name_length > 0) {
                    col_name_str = std::string(column_name, column_name + column_name_length);
                }
                cols.emplace_back(TableColumn(ordinal++, hidden, col_name_str));
            }
            sqlite3_finalize(stmt);
        }

        return cols;
    }

    bool is_valid_draft_type_name(std::string_view type_name) {
        if (type_name == "string") {
            return true;
        }
        if (type_name == "number") {
            return true;
        }
        if (type_name == "integer") {
            return true;
        }

        if (type_name == "boolean") {
            return true;
        }

        if (type_name == "null") {
            return true;
        }

        if (type_name == "object") {
            return true;
        }

        if (type_name == "array") {
            return true;
        }

        return false;
    }

    bool schema_supports_type(json schema, std::string_view type_name) {
        
        if (!is_valid_draft_type_name(type_name)) {
            return false;
        }

        if(schema.is_null()) {
            // No schema supports any type
            return true;
        }

        if (schema.is_string()) {
            if (schema.empty()) {
                // TODO: confirm empty type string means any type is supported
                return true;
            }
            return schema.get<std::string>() == type_name;
        }

        json type = schema.value("type", json{});
        if (type.is_null()) {
            return true;
        }

        if (type.is_null()) {
            return true;
        }

        if (type.is_string()) {
            
            if (type.size() == 0) {
                // TODO: confirm empty type string means any type is supported
                return true;
            }

            return type_name == type.get<std::string>();
        }

        if (type.is_array()) {
            
            if (type.size() == 0) {
                // TODO: confirm empty type array means any type is supported
                return true;
            }

            for (auto tn = type.begin(); tn != type.end(); ++tn) {
                if (type_name == tn->get<std::string>()) {
                    return true;
                }
            }
        }
        
        return false;
    }

    json value_to_json(sqlite3_value* value) {
        if (value == nullptr) {
            return nullptr;
        }
        const auto val_type = sqlite3_value_type(value);

        switch (val_type) {
            case SQLITE_INTEGER:
                return json(sqlite3_value_int64(value));

            case SQLITE_FLOAT:
                return json(sqlite3_value_double(value));

            case SQLITE_TEXT: {
                const auto* text = reinterpret_cast<const char*>(sqlite3_value_text(value));
                const int length = sqlite3_value_bytes(value);

                if (text != nullptr && length > 0) {
                    return json(std::string_view(text, static_cast<size_t>(length)));
                }
                return json("");
            }

            case SQLITE_BLOB: {
                const auto* data = static_cast<const uint8_t*>(sqlite3_value_blob(value));
                const int length = sqlite3_value_bytes(value);

                if (data != nullptr && length > 0) {
                    return json::binary(std::vector<uint8_t>(data, data + length));
                }
                return json::binary({});
            }

            case SQLITE_NULL:
                return nullptr;

            default:
                throw std::invalid_argument("Unsupported SQLite type: " + std::to_string(val_type));
        }
    }

    bool are_values_equal(sqlite3_value* a, sqlite3_value* b) {
        int typeA = sqlite3_value_type(a);
        int typeB = sqlite3_value_type(b);
        if (typeA != typeB) return false;

        switch (typeA) {
            case SQLITE_INTEGER:
                return sqlite3_value_int64(a) == sqlite3_value_int64(b);
            case SQLITE_FLOAT:
                return sqlite3_value_double(a) == sqlite3_value_double(b);
            case SQLITE_TEXT: {
                int lenA = sqlite3_value_bytes(a);
                int lenB = sqlite3_value_bytes(b);
                if (lenA != lenB) return false;
                return std::memcmp(sqlite3_value_text(a), sqlite3_value_text(b), lenA) == 0;
            }
            case SQLITE_BLOB: {
                int lenA = sqlite3_value_bytes(a);
                int lenB = sqlite3_value_bytes(b);
                if (lenA != lenB) return false;
                return std::memcmp(sqlite3_value_blob(a), sqlite3_value_blob(b), lenA) == 0;
            }
            case SQLITE_NULL:
                return true;
            default:
                return false;
        }
    }

    /*
    void prevalidate_json_schema(void* user_data, sqlite3* db_handle, int op_code, const char* db_name, const char* table_name, sqlite3_int64 key1, sqlite3_int64 key2) {
        if (op_code != SQLITE_INSERT && op_code != SQLITE_UPDATE) {
            return;
        }

        auto db = static_cast<SemanticDatabase*>(user_data);
        if (db == nullptr) {
            return;
        }

        std::string table_name_str = table_name;
        sqlite3_stmt* meta_stmt = nullptr;
        sqlite3_value* val = nullptr;
        json schema = nullptr;

        auto columns = get_table_columns(db_handle, table_name_str);
        
        if (table_name_str == "table_json_schema") {
            // TODO: Validate that all records of the table match the schema
            return;
        }

        // Ignore internal schema tables
        if (table_name_str == "json_schema_store") {

            // Locate the ordinal of "schema_json"
            int schemaJsonOrdinal = -1;
            for (int i = 0; i < columns.size(); ++i) {
                if (columns[i].name == "schema_json") {
                    schemaJsonOrdinal = columns[i].ordinal;   
                    break;                 
                }
            }

            // It would be VERY unexpected for this to be -1
            if(schemaJsonOrdinal != -1) {

                // Get the new schema_json value
                sqlite3_preupdate_new(db_handle, schemaJsonOrdinal, &val);
                if (val == nullptr || sqlite3_value_type(val) != SQLITE_TEXT) {
                    throw SQLite::Exception("JSON schema must be a non-blank string");
                }
    
                // Retrieve the schema string
                auto schema_chars = reinterpret_cast<const char*>(sqlite3_value_text(val));
                auto schema_length = sqlite3_value_bytes(val);
                auto schema_str = std::string_view(schema_chars, schema_chars + schema_length);
                schema = parse_json(schema_str);
                
                validate_schema(schema);            
            }
            
            // TODO: ensure any tables registered for this URI's rows are ALL valid
            return;
        }

        // Build the prospective complete old and new row objects
        json row_new = nullptr;
        json row_old = nullptr;
        std::vector<std::string> impacted_columns;
        std::map<std::string,json> schema_cache;

        // both insert and update have a "new" row
        row_new = json::object();
        
        // update has an "old" row; insert does not
        if (op_code == SQLITE_UPDATE) {
            row_old = json::object();
        }

        // Populate old/new rows
        for (int i = 0; i < columns.size(); ++i) {
            auto& column = columns[i];

            sqlite3_value* oldVal = nullptr;
            sqlite3_value* newVal = nullptr;

            if(op_code == SQLITE_UPDATE) {
                sqlite3_preupdate_old(db_handle, column.ordinal, &oldVal);
            }
            
            sqlite3_preupdate_new(db_handle, column.ordinal, &newVal);
            
            int old_type = SQLITE_NULL;
            if (oldVal != nullptr) {
                old_type = sqlite3_value_type(oldVal);
            }
            
            int new_type = SQLITE_NULL;
            if (newVal != nullptr) {
                new_type = sqlite3_value_type(newVal);
            }

            if(op_code == SQLITE_UPDATE) {
                sqlite3_preupdate_old(db_handle, column.ordinal, &oldVal);
                row_old[column.name] = value_to_json(oldVal);
            }
            
            sqlite3_preupdate_new(db_handle, column.ordinal, &newVal);
            
            row_new[column.name] = value_to_json(newVal);

            if ((op_code != SQLITE_UPDATE) || !are_values_equal(oldVal, newVal)) {
                // Ignore virtual fields (2), but take normal(0), hidden (1), and stored(3) fields
                if (column.hidden != 2) {
                    impacted_columns.push_back(column.name);
                }
            }
        }

        // Validate the schema
        schema = db->get_table_schema(table_name_str);
        if (schema.is_null()) {
            // No schema -> nothing to validate
            return;
        }

        // Transform string field values to object field values where type supports object and not string
        // NOTE: This does not check for object/string support any deeper than root declarations of fields
        // it does not check the type of any conditional blocks or other advanced constructs.
        auto properties = schema["properties"];
        if (properties != nullptr && properties.is_object()) {

            // Find and convert object-typed properties
            for (auto begin_it = impacted_columns.begin(); begin_it != impacted_columns.end(); ++begin_it) {
                auto column_name = *begin_it;
                json column_def = properties.value(column_name, json{});
                
                if (column_def != nullptr) {

                    // Flags indicating string or object support
                    bool supports_object = schema_supports_type(column_def, "object");
                    bool supports_string = schema_supports_type(column_def, "string");

                    if (supports_object && !supports_string) {
                        
                        // Convert the object string to JSON for the old row
                        if (row_old != nullptr) {
                            auto current_val = row_old.value(column_name, json{});
                            if (current_val.is_string()) {
                                row_old[column_name] = parse_json(current_val.get<std::string>());
                            }
                        }

                        // Convert the object string to JSON for the new row
                        if (row_new != nullptr) {
                            auto current_val = row_new.value(column_name, json{});
                            if (current_val.is_string()) {
                                row_new[column_name] = parse_json(current_val.get<std::string>());
                            }
                        }
                    }
                }
            }
        }

        // Perform validation per the schema
        try {
            json_validator validator;
            validator.set_root_schema(schema);
            validator.validate(row_new);
        }
        catch (const std::exception& e) {
            
            throw SQLite::Exception(std::string("Schema validation error on ") + table_name_str + ": " + e.what(), SQLITE_CONSTRAINT);
        }
    }*/

    #if INCLUDE_USERS_EXAMPLE

    void usersExample(SemanticDatabase& db) {

        // TODO: Complete remove users example when there's a couple things successfully using this
        
        db.get_Database().exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, details TEXT);");
        
        db.insert_json_schema(EXAMPLE_User_Schema);
        db.set_table_schema("users", "https://example.com/schemas/user.schema.json");

        try {
            db.get_Database().exec("INSERT INTO users (name, details) VALUES ('Fred', '{ \"favorite_color\": \"blue\" }');");
        }
        catch (SQLite::Exception & e) {
            std::cerr << "Unexpected error inserting Fred: " << e.what() << std::endl;
        }
        try{
            db.get_Database().exec("INSERT INTO users (name) VALUES ('Alice');");
        }
        catch(SQLite::Exception& e) {
            std::cerr << "As expected, Alice was forbidden: " << e.what() << std::endl;
        }

        // Query data
        SQLite::Statement query(db.get_Database(), "SELECT name FROM users WHERE id = 1;");
        if (query.executeStep()) {
            std::string name = query.getColumn(0);
            std::cout << "User found: " << name << std::endl;
        }
        query.reset();
    }
    #endif

    SemanticDatabase::SemanticDatabase(std::string path):
        database(path.data(), SQLite::OPEN_READWRITE, SQLite::OPEN_CREATE) {
        
        // 2. Hand-register the static module to this connection
        int rc = sqlite3_vec_init(this->database.getHandle(), nullptr, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to initialize static sqlite-vec module.");
        }
        
        // TODO: figure out what we really want for embeddings table(s) later on (maybe driven by schema extension prop x-semantic-embed or something?)
        // use nomic-embed-en-v1.5 (8k context window, low mem footprint, matryoshka support) 
        // OR use bge-m3 or bge-large-en-v1.5 (Dense, robust, semantic retrieval)
        this->database.exec("CREATE VIRTUAL TABLE vec_items USING vec0(embedding float[1536]);");
        this->database.exec(CREATE_TABLE_json_schema_store);
        this->database.exec(CREATE_TABLE_table_json_schema);
        
        //sqlite3_preupdate_hook(this->database.getHandle(), &prevalidate_json_schema, this);

        #if INCLUDE_USERS_EXAMPLE

        usersExample(*this);

        #endif // Users Example
    }

    SemanticDatabase::~SemanticDatabase() {
        //sqlite3_preupdate_hook(this->database.getHandle(), nullptr, nullptr);
        
        // TODO: This feels hack - look into when/why/if this is needed and if its actually dangerous
        // Finalize any aborted in-flight statement so SQLiteCpp destructs cleanly
        sqlite3_stmt* stmt = nullptr;
        while ((stmt = sqlite3_next_stmt(this->database.getHandle(), nullptr)) != nullptr) {
            std::cerr << "Statement still open: " << std::endl << sqlite3_sql(stmt) << std::endl;
            sqlite3_finalize(stmt);
        }
    }

    SQLite::Database& SemanticDatabase::get_Database() {
        return this->database;
    }

    class sqlite3_stmt_finalizer {
        public:
    
        sqlite3_stmt *stmt;
        sqlite3_stmt_finalizer(sqlite3_stmt* stmt):stmt(stmt) {}

        virtual ~sqlite3_stmt_finalizer() {
            if (this->stmt != nullptr) {
                sqlite3_finalize(this->stmt);
                this->stmt = nullptr;
            }
        }
    
    };

    json get_table_schema(sqlite3* db, std::string table_name, std::map<std::string, json>* schema_cache = nullptr) {
        if (schema_cache != nullptr) {
            auto existing = schema_cache->find(table_name);
            if (existing != schema_cache->end()) {
                return existing->second;
            }
        }
        
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db, SELECT_table_schema_by_table_name.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_stmt_finalizer deferred_finalize(stmt);

            sqlite3_bind_text(stmt, 1, table_name.data(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto schema_length = sqlite3_column_bytes(stmt, 0);
                auto schema_chars = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (schema_chars && schema_length > 0) {
                    auto schema = parse_json(std::string_view(schema_chars, schema_chars + schema_length));
                    if (schema_cache != nullptr && !schema.is_null()) {
                        schema_cache->at(table_name) = schema;
                    }
                    return schema;
                }
            }
        }
        return nullptr;
    }

    json SemanticDatabase::get_table_schema(std::string table_name) {
        return semantic_database::get_table_schema(this->database.getHandle(), table_name);
    }

    void SemanticDatabase::insert_json_schema(std::string schema_json) {
        auto schema = parse_json(schema_json);
        this->insert_json_schema(schema);
    }

    void SemanticDatabase::insert_json_schema(json schema_json) {
        validate_schema(schema_json);

        SQLite::Statement insert_schema(this->database, INSERT_INTO_json_schema_store);
        auto schema_json_str = schema_json.dump();
        insert_schema.bind(":schema_json", schema_json_str);
        insert_schema.exec();
        insert_schema.reset();
    }

    void SemanticDatabase::set_table_schema(std::string table_name, std::string schema_uri) {
        SQLite::Statement insert_schema_assign(this->database, INSERT_INTO_table_json_schema);
        insert_schema_assign.bind(":table_name", "users");
        insert_schema_assign.bind(":schema_uri", schema_uri);
        insert_schema_assign.exec();
        insert_schema_assign.reset();
    }

    json read_result_json(sqlite3_stmt *stmt, json schema, std::map<std::string, json>* schema_cache = nullptr) {
        if(stmt == nullptr) {
            return nullptr;
        }

        auto column_count = sqlite3_column_count(stmt);
        if (column_count == 0) {
            return nullptr;
        }

        json result = json::object();

        for (auto i = 0; i < column_count; i++) {
            auto column_name = sqlite3_column_name(stmt, i);
            auto column_type = sqlite3_column_type(stmt, i);
            auto column_value = sqlite3_column_value(stmt, i);
            auto column_name_str = std::string(column_name);

            switch(column_type) {
                
                case SQLITE_NULL:
                result[column_name_str] = json{};
                break;

                case SQLITE_FLOAT:
                result[column_name_str] = json(sqlite3_value_double(column_value));
                break;
                
                case SQLITE_INTEGER:
                result[column_name_str] = json(sqlite3_value_int(column_value));
                break;

                case SQLITE3_TEXT: {
                    auto str_length = sqlite3_value_bytes(column_value);
                    auto str_chars = reinterpret_cast<const char*>(sqlite3_value_text(column_value));
                    std::string_view str_value = "";                    
                    if (str_chars != nullptr && str_length > 0) {
                        str_value = std::string_view(str_chars, str_chars + str_length);
                    }

                    auto table_name = sqlite3_column_table_name(stmt, i);
                    auto table_name_str = std::string(table_name);
                    if (table_name) {
                        
                        auto orig_field_name = sqlite3_column_origin_name(stmt, i);
                        auto table_schema = get_table_schema(sqlite3_db_handle(stmt), table_name_str, schema_cache);

                        if(!table_schema.is_null() && table_schema.is_object()) {
                            auto properties = table_schema.at("properties");

                            if (!properties.is_null() && properties.is_object()) {
                                auto column_prop = properties.at(column_name);
                                if (!column_prop.is_null()) {
                                    bool supports_object = schema_supports_type(column_prop, "object");
                                    bool supports_string = schema_supports_type(column_prop, "string");

                                    if (supports_object && !supports_string && !str_value.empty()) {
                                        auto column_value_json = parse_json(str_value);
                                        result[column_name_str] = column_value_json;
                                        continue;
                                    }
                                }
                            }
                        }
                    }

                    result[column_name_str] = json(str_value);
                }
                break;

                case SQLITE_BLOB: {
                    auto blob_length = sqlite3_value_bytes(column_value);
                    auto blob_bytes = static_cast<const uint8_t*>(sqlite3_value_blob(column_value));
                    
                    std::vector<uint8_t> blob_byte_vec;
                    if (blob_length > 0 && blob_bytes != nullptr) {
                        blob_byte_vec = std::vector<uint8_t>(blob_bytes, blob_bytes + blob_length);
                    }

                    result[column_name_str] = json::binary(blob_byte_vec);
                }
                break;
                
                default:
                    throw SQLite::Exception("Unsupported SQLite column type for :" + column_name_str);
            };
        }

        return result;
    }

    json SemanticDatabase::read_result_json(SQLite::Statement& stmt) {
        
        auto column_count = stmt.getColumnCount();
        if (column_count == 0) {
            return nullptr;
        }

        json result = json::object();

        for (auto i = 0; i < column_count; i++) {
            auto column_name = stmt.getColumnName(i);
            auto column_value = stmt.getColumn(i);
            
            auto column_name_str = std::string(column_name);
            
            switch(column_value.getType()) {
                
                case SQLITE_NULL:
                result[column_name_str] = json{};
                break;

                case SQLITE_FLOAT:
                result[column_name_str] = json(column_value.getDouble());
                break;
                
                case SQLITE_INTEGER:
                // TODO: getInt64? or is that another type?
                result[column_name_str] = json(column_value.getInt());
                break;

                case SQLITE3_TEXT: {

                    // Get the string value
                    auto str_length = column_value.getBytes();
                    auto str_chars = reinterpret_cast<const char*>(column_value.getText());
                    std::string_view str_value = "";                    
                    if (str_chars != nullptr && str_length > 0) {
                        str_value = std::string_view(str_chars, str_chars + str_length);
                    }

                    auto table_name = column_value.getTableName();
                    if (table_name) {
                        auto table_name_str = std::string(table_name);
                        auto orig_field_name = column_value.getOriginName();
                        auto table_schema = semantic_database::get_table_schema(this->database.getHandle(), table_name_str, &this->schema_cache);
                        
                        if(!table_schema.is_null() && table_schema.is_object() && orig_field_name != nullptr) {
                            auto properties = table_schema.at("properties");

                            if (!properties.is_null() && properties.is_object()) {
                                json column_prop;
                                column_prop = properties.at(orig_field_name);
                                if (!column_prop.is_null()) {
                                    bool supports_object = schema_supports_type(column_prop, "object");
                                    bool supports_string = schema_supports_type(column_prop, "string");

                                    if (supports_object && !supports_string && !str_value.empty()) {
                                        auto column_value_json = parse_json(str_value);
                                        result[column_name_str] = column_value_json;
                                        continue;
                                    }
                                }
                            }
                        }
                    }

                    result[column_name_str] = json(str_value);
                }
                break;

                case SQLITE_BLOB: {
                    auto blob_length = column_value.getBytes();
                    auto blob_bytes = static_cast<const uint8_t*>(column_value.getBlob());
                    
                    std::vector<uint8_t> blob_byte_vec;
                    if (blob_length > 0 && blob_bytes != nullptr) {
                        blob_byte_vec = std::vector<uint8_t>(blob_bytes, blob_bytes + blob_length);
                    }

                    result[column_name_str] = json::binary(blob_byte_vec);
                }
                break;
                
                default:
                    throw SQLite::Exception("Unsupported SQLite column type for :" + column_name_str);
            };
        }

        return result;
    }
}