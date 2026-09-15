#pragma once

#include "SQLiteCpp/SQLiteCpp.h"
#include "nlohmann/json-schema.hpp"

#include <map>

#define SEMANTIC_DATABASE_LOCATION_MEMORY ":memory:"

using nlohmann::json;
using nlohmann::json_schema::json_validator;

namespace semantic_database {
    
    class SemanticDatabase {
        public:
            SemanticDatabase(std::string path = SEMANTIC_DATABASE_LOCATION_MEMORY);
            virtual ~SemanticDatabase();
            SQLite::Database& get_Database();

            json get_table_schema(std::string table_name);

            void insert_json_schema(std::string json_schema);
            void insert_json_schema(json json_schema);
            void set_table_schema(std::string table_name, std::string schema_uri);
            json read_result_json(SQLite::Statement& statement);
            

        private:
            SQLite::Database database;
            std::map<std::string, json> schema_cache;
    };
}
