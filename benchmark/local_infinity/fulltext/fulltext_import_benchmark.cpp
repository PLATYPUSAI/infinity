// Copyright(C) 2023 InfiniFlow, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include <cstring>
#include <string>
#include <tuple>
#include <unistd.h>
#include <getopt.h>

import stl;
import third_party;
import compilation_config;
import local_file_system;
import profiler;
import infinity;

import internal_types;
import logical_type;
import create_index_info;
import column_def;
import data_type;
import query_options;
import extra_ddl_info;
import statement_common;
import parsed_expr;
import constant_expr;
import logger;

using namespace infinity;

void ReadJsonl(std::ifstream &input_file, int lines_to_read, Vector<Tuple<char *, char *, char *>> &batch) {
    String line;
    int lines_readed = 0;
    batch.clear();
    static const char *columns[3] = {"id", "title", "text"};
    while (lines_readed < lines_to_read) {
        line.clear();
        std::getline(input_file, line);
        if (input_file.eof())
            break;
        else if (line.length() == 0)
            continue;
        else {
            std::string_view json_sv(line);
            nlohmann::json json = nlohmann::json::parse(json_sv);
            char *elems[3];
            for (SizeT i = 0; i < 3; i++) {
                assert(json.contains(columns[i]));
                String val_str = json[columns[i]];
                char *val_buf = (char *)malloc(val_str.length() + 1);
                memcpy(val_buf, val_str.data(), val_str.length());
                val_buf[val_str.length()] = '\0';
                elems[i] = val_buf;
            }
            batch.push_back({elems[0], elems[1], elems[2]});
            lines_readed++;
        }
    }
}

SharedPtr<Infinity> CreateDbAndTable(const String &db_name, const String &table_name) {
    Vector<ColumnDef *> column_defs;
    {
        String col1_name = "id";
        auto col1_type = std::make_shared<DataType>(LogicalType::kVarchar);
        auto col1_def = new ColumnDef(0, col1_type, std::move(col1_name), std::unordered_set<ConstraintType>());
        column_defs.push_back(col1_def);
    }
    {
        String col2_name = "title";
        auto col2_type = std::make_shared<DataType>(LogicalType::kVarchar);
        auto col2_def = new ColumnDef(0, col2_type, std::move(col2_name), std::unordered_set<ConstraintType>());
        column_defs.push_back(col2_def);
    }
    {
        String col3_name = "text";
        auto col3_type = std::make_shared<DataType>(LogicalType::kVarchar);
        auto col3_def = new ColumnDef(0, col3_type, std::move(col3_name), std::unordered_set<ConstraintType>());
        column_defs.push_back(col3_def);
    }

    String data_path = "/tmp/infinity";

    Infinity::LocalInit(data_path);
    // SetLogLevel(LogLevel::kTrace);

    SharedPtr<Infinity> infinity = Infinity::LocalConnect();
    CreateDatabaseOptions create_db_options;
    create_db_options.conflict_type_ = ConflictType::kIgnore;
    infinity->CreateDatabase(db_name, std::move(create_db_options));

    DropTableOptions drop_tb_options;
    drop_tb_options.conflict_type_ = ConflictType::kIgnore;
    infinity->DropTable(db_name, table_name, std::move(drop_tb_options));

    CreateTableOptions create_tb_options;
    create_tb_options.conflict_type_ = ConflictType::kIgnore;
    infinity->CreateTable(db_name, table_name, std::move(column_defs), Vector<TableConstraint *>{}, std::move(create_tb_options));
    return infinity;
}

void BenchmarkImport(SharedPtr<Infinity> infinity,
                     const String &db_name,
                     const String &table_name,
                     const String &import_from) {
    LocalFileSystem fs;
    if (!fs.Exists(import_from)) {
        LOG_ERROR(fmt::format("Data file doesn't exist: {}", import_from));
        return;
    }

    BaseProfiler profiler;

    profiler.Begin();
    ImportOptions import_options;
    import_options.copy_file_type_ = CopyFileType::kJSONL;
    infinity->Import(db_name, table_name, import_from, std::move(import_options));
    LOG_INFO(fmt::format("Import data cost: {}", profiler.ElapsedToString()));
    profiler.End();
}

void BenchmarkInsert(SharedPtr<Infinity> infinity, const String &db_name, const String &table_name, const String &insert_from) {
    std::ifstream input_file(insert_from);
    if (!input_file.is_open()) {
        LOG_ERROR(fmt::format("Failed to open file {}", insert_from));
        return;
    }

    BaseProfiler profiler;

    profiler.Begin();
    SizeT num_rows = 0;
    const int batch_size = 100;
    Vector<Tuple<char *, char *, char *>> batch;
    batch.reserve(batch_size);

    Vector<String> orig_columns{"id", "title", "text"};
    bool done = false;
    ConstantExpr *const_expr = nullptr;
    while (!done) {
        ReadJsonl(input_file, batch_size, batch);
        if (batch.empty()) {
            done = true;
            break;
        }
        num_rows += batch.size();
        Vector<String> *columns = new Vector<String>(orig_columns);
        Vector<Vector<ParsedExpr *> *> *values = new Vector<Vector<ParsedExpr *> *>();
        for (auto &t : batch) {
            values->reserve(batch_size);
            auto value_list = new Vector<ParsedExpr *>(columns->size());
            const_expr = new ConstantExpr(LiteralType::kString);
            const_expr->str_value_ = std::get<0>(t);
            value_list->at(0) = const_expr;
            const_expr = new ConstantExpr(LiteralType::kString);
            const_expr->str_value_ = std::get<1>(t);
            value_list->at(1) = const_expr;
            const_expr = new ConstantExpr(LiteralType::kString);
            const_expr->str_value_ = std::get<2>(t);
            value_list->at(2) = const_expr;
            values->push_back(value_list);
        }
        infinity->Insert(db_name, table_name, columns, values);

        // NOTE: ~InsertStatement() has deleted or freed columns, values, value_list, const_expr, const_expr->str_value_
        if (batch.size() < batch_size) {
            done = true;
            break;
        }
    }
    input_file.close();
    LOG_INFO(fmt::format("Insert data {} rows cost: {}", num_rows, profiler.ElapsedToString()));
    profiler.End();
}

void BenchmarkCreateIndex(SharedPtr<Infinity> infinity,
                          const String &db_name,
                          const String &table_name,
                          const String &index_name) {
    BaseProfiler profiler;
    profiler.Begin();
    auto index_info_list = new Vector<IndexInfo *>();
    auto index_info = new IndexInfo();
    index_info->index_type_ = IndexType::kFullText;
    index_info->column_name_ = "text";
    index_info->index_param_list_ = new Vector<InitParameter *>();
    index_info_list->push_back(index_info);

    auto r = infinity->CreateIndex(db_name, table_name, index_name, index_info_list, CreateIndexOptions());
    if (r.IsOk()) {
        r = infinity->Flush();
    } else {
        LOG_ERROR(fmt::format("Fail to create index {}", r.ToString()));
        return;
    }

    // NOTE: ~CreateStatement() has already deleated or freed index_info_list, index_info, index_info->index_param_list_.
    LOG_INFO(fmt::format("Create index cost: {}", profiler.ElapsedToString()));
    profiler.End();
}

void BenchmarkOptimize(SharedPtr<Infinity> infinity, const String &db_name, const String &table_name) {
    BaseProfiler profiler;
    profiler.Begin();
    infinity->Optimize(db_name, table_name);
    LOG_INFO(fmt::format("Merge index cost: {}", profiler.ElapsedToString()));
    profiler.End();
}

bool Parse(int argc, char* argv[], bool& is_import, bool& is_insert, bool& is_merge) {
    if (argc < 2) {
        return true;
    }

    is_import = false;
    is_insert = false;
    is_merge = false;

    int opt;
    static struct option long_options[] = {
        {"import", no_argument, 0, 'i'},
        {"insert", no_argument, 0, 'r'},
        {"merge", no_argument, 0, 'm'},
        {0, 0, 0, 0}
    };

    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "irm", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                is_import = true;
                break;
            case 'r':
                is_insert = true;
                break;
            case 'm':
                is_insert = true;
                is_merge = true;
                break;
            default:
                LOG_ERROR(fmt::format("Usage: {} [--import | -i] [--insert | -r] [--merge | -m]", argv[0]));
                return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    // Usage: ./fulltext_import_benchmark [--import | -i] [--insert | -r] [--merge | -m]
    // No arguments will run all tests for debugging
    String db_name = "default";
    String table_name = "ft_dbpedia_benchmark";
    String index_name = "ft_dbpedia_index";
    String srcfile = test_data_path();
    srcfile += "/benchmark/dbpedia-entity/corpus.jsonl";

    SharedPtr<Infinity> infinity = CreateDbAndTable(db_name, table_name);
    bool is_import = true;
    bool is_insert = true;
    bool is_merge = true;

    if (!Parse(argc, argv, is_import, is_insert, is_merge)) {
        return 1;
    }

    if (is_import) {
        BenchmarkImport(infinity, db_name, table_name, srcfile);
    }
    if (is_insert) {
        BenchmarkCreateIndex(infinity, db_name, table_name, index_name);
        BenchmarkInsert(infinity, db_name, table_name, srcfile);
    }
    if (is_merge) {
        BenchmarkOptimize(infinity, db_name, table_name);
    }
    sleep(10);
    Infinity::LocalUnInit();
}