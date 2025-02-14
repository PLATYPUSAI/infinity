// Copyright(C) 2024 InfiniFlow, Inc. All rights reserved.
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
module;

#include <cstring>
#include <iostream>

module http_server;

import infinity;
import stl;
import status;
import third_party;
import defer_op;
import data_block;
import data_table;
import data_type;
import value;
import infinity_exception;
import logger;
import query_result;
import query_options;
import column_vector;
import infinity_context;
import query_context;
import column_def;
import internal_types;
import parsed_expr;
import constant_expr;
import expr_parser;
import expression_parser_result;
import create_index_info;
import statement_common;
import extra_ddl_info;
import update_statement;
import http_search;

namespace {

using namespace infinity;

class ListDatabaseHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto result = infinity->ListDatabases();
        nlohmann::json json_response;
        HTTPStatus http_status;
        if (result.IsOk()) {
            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                for (int i = 0; i < row_count; ++i) {
                    Value value = data_block->GetValue(0, i);
                    const String &db_name = value.GetVarchar();
                    json_response["databases"].push_back(db_name);
                }
            }
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class CreateDatabaseHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        // get database name
        String database_name = request->getPathVariable("database_name");

        // get create option
        String body_info = request->readBodyToString();
        nlohmann::json body_info_json = nlohmann::json::parse(body_info);
        String option = body_info_json["create_option"];
        CreateDatabaseOptions create_option;

        // create database
        auto result = infinity->CreateDatabase(database_name, create_option);

        HTTPStatus http_status;
        nlohmann::json json_response;
        if (result.IsOk()) {
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class DropDatabaseHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        // get database name
        String database_name = request->getPathVariable("database_name");

        // get drop option
        String body_info = request->readBodyToString();
        nlohmann::json body_info_json = nlohmann::json::parse(body_info);
        String option = body_info_json["drop_option"];
        DropDatabaseOptions drop_option;

        auto result = infinity->DropDatabase(database_name, drop_option);

        nlohmann::json json_response;
        HTTPStatus http_status;
        if (result.IsOk()) {
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowDatabaseHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto result = infinity->ShowDatabase(database_name);

        nlohmann::json json_response;
        nlohmann::json json_res;
        HTTPStatus http_status;

        if (result.IsOk()) {
            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                auto column_cnt = result.result_table_->ColumnCount();

                for (int row = 0; row < row_count; ++row) {
                    nlohmann::json json_database;
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        Value value = data_block->GetValue(col, row);
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        const String &column_value = value.ToString();
                        json_database[column_name] = column_value;
                    }
                    json_res["res"].push_back(json_database);
                }
                for (auto &element : json_res["res"]) {
                    json_response[element["name"]] = element["value"];
                }
            }

            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class CreateTableHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        String database_name = request->getPathVariable("database_name");
        String table_name = request->getPathVariable("table_name");

        String body_info = request->readBodyToString();
        nlohmann::json body_info_json = nlohmann::json::parse(body_info);

        auto fields = body_info_json["fields"];
        auto properties = body_info_json["properties"];

        nlohmann::json json_response;
        HTTPStatus http_status;

        Vector<ColumnDef *> column_definitions;
        i64 id = 0;

        for (auto &field : fields) {
            for (auto &field_element : field.items()) {
                String column_name = field_element.key();
                auto values = field_element.value();
                String value_type = values["type"];
                ToLower(value_type);
                SharedPtr<DataType> column_type = DataType::StringDeserialize(value_type);
                if (column_type) {
                    HashSet<ConstraintType> constraints;
                    for (auto &constraint_json : values["constraints"]) {
                        String constraint = constraint_json;
                        ToLower(constraint);
                        constraints.insert(StringToConstraintType(constraint));
                    }
                    ColumnDef *col_def = new ColumnDef(id++, column_type, column_name, constraints);
                    column_definitions.emplace_back(col_def);
                } else {
                    infinity::Status status = infinity::Status::NotSupport(fmt::format("{} type is not supported yet.", values["type"]));
                    json_response["error_code"] = status.code();
                    json_response["error_message"] = status.message();
                    HTTPStatus http_status;
                    http_status = HTTPStatus::CODE_500;
                    return ResponseFactory::createResponse(http_status, json_response.dump());
                }
            }
        }
        Vector<TableConstraint *> table_constraint;
        CreateTableOptions create_table_opts;

        auto result = infinity->CreateTable(database_name, table_name, column_definitions, table_constraint, create_table_opts);

        if (result.IsOk()) {
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class DropTableHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        String database_name = request->getPathVariable("database_name");
        String table_name = request->getPathVariable("table_name");
        DropTableOptions drop_table_opts;
        auto result = infinity->DropTable(database_name, table_name, drop_table_opts);

        HTTPStatus http_status;
        http_status = HTTPStatus::CODE_200;
        nlohmann::json json_response;
        if (result.IsOk()) {
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ListTableHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        String database_name = request->getPathVariable("database_name");
        auto result = infinity->ShowTables(database_name);
        nlohmann::json json_response;
        HTTPStatus http_status;
        if (result.IsOk()) {
            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                auto column_cnt = result.result_table_->ColumnCount();
                for (int row = 0; row < row_count; ++row) {
                    nlohmann::json json_table;
                    for (SizeT col = 1; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_table[column_name] = column_value;
                    }
                    json_response["tables"].push_back(json_table);
                }
            }

            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowTableHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        String database_name = request->getPathVariable("database_name");
        String table_name = request->getPathVariable("table_name");

        auto result = infinity->ShowTable(database_name, table_name);
        nlohmann::json json_response;
        nlohmann::json json_res;
        HTTPStatus http_status;
        if (result.IsOk()) {
            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                auto column_cnt = result.result_table_->ColumnCount();
                for (int row = 0; row < row_count; ++row) {
                    nlohmann::json json_table;
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_table[column_name] = column_value;
                    }
                    json_res["tables"].push_back(json_table);
                }
                for (auto &element : json_res["tables"]) {
                    json_response[element["name"]] = element["value"];
                }
            }
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowTableColumnsHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        String database_name = request->getPathVariable("database_name");
        String table_name = request->getPathVariable("table_name");

        auto result = infinity->ShowColumns(database_name, table_name);
        nlohmann::json json_response;
        HTTPStatus http_status;
        if (result.IsOk()) {
            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                auto column_cnt = result.result_table_->ColumnCount();
                for (int row = 0; row < row_count; ++row) {
                    nlohmann::json json_table;
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_table[column_name] = column_value;
                    }
                    json_response["columns"].push_back(json_table);
                }
            }
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class InsertHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        nlohmann::json json_response;
        HTTPStatus http_status = HTTPStatus::CODE_500;

        String data_body = request->readBodyToString();
        try {
            nlohmann::json http_body_json = nlohmann::json::parse(data_body);

            SizeT row_count = http_body_json.size();
            if (http_body_json.is_array() && row_count > 0) {

                // First row
                const auto &first_row_json = http_body_json[0];
                SizeT column_count = first_row_json.size();

                // Used for the column type validation
                HashMap<String, LiteralType> column_type_map;
                HashMap<String, u64> column_name_id_map;

                // inserted columns
                Vector<Vector<ParsedExpr *> *> *column_values = new Vector<Vector<infinity::ParsedExpr *> *>();
                column_values->reserve(row_count);
                DeferFn defer_free_column_values([&]() {
                    if (column_values != nullptr) {
                        for (auto &value_array : *column_values) {
                            for (auto &value_ptr : *value_array) {
                                delete value_ptr;
                                value_ptr = nullptr;
                            }
                            delete value_array;
                            value_array = nullptr;
                        }
                        delete column_values;
                        column_values = nullptr;
                    }
                });

                Vector<String> *columns = new Vector<String>();
                column_type_map.reserve(column_count);
                column_name_id_map.reserve(column_count);
                columns->reserve(column_count);
                DeferFn defer_free_columns([&]() {
                    if (columns != nullptr) {
                        delete columns;
                        columns = nullptr;
                    }
                });

                // First row
                {
                    Vector<ParsedExpr *> *values_row = new Vector<infinity::ParsedExpr *>();
                    DeferFn defer_free_value_row([&]() {
                        if (values_row != nullptr) {
                            for (auto &value_ptr : *values_row) {
                                delete value_ptr;
                                value_ptr = nullptr;
                            }
                            delete values_row;
                            values_row = nullptr;
                        }
                    });

                    for (const auto &item : first_row_json.items()) {
                        const auto &key = item.key();
                        auto iter = column_type_map.find(key);
                        if (iter != column_type_map.end()) {
                            json_response["error_code"] = ErrorCode::kDuplicateColumnName;
                            json_response["error_message"] = fmt::format("Duplicated column name: {}", key);
                            return ResponseFactory::createResponse(http_status, json_response.dump());
                        }
                        column_name_id_map.emplace(key, columns->size());
                        columns->emplace_back(key);

                        const auto &value = item.value();
                        switch (value.type()) {
                            case nlohmann::json::value_t::boolean: {
                                auto bool_value = value.template get<bool>();
                                column_type_map.emplace(key, LiteralType::kBoolean);

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kBoolean);
                                const_expr->bool_value_ = bool_value;
                                values_row->emplace_back(const_expr);
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::number_integer: {
                                auto integer_value = value.template get<i64>();
                                column_type_map.emplace(key, LiteralType::kInteger);

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kInteger);
                                const_expr->integer_value_ = integer_value;
                                values_row->emplace_back(const_expr);
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::number_unsigned: {
                                auto integer_value = value.template get<u64>();
                                column_type_map.emplace(key, LiteralType::kInteger);

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kInteger);
                                const_expr->integer_value_ = integer_value;
                                values_row->emplace_back(const_expr);
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::number_float: {
                                auto float_value = value.template get<f64>();
                                column_type_map.emplace(key, LiteralType::kDouble);

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kDouble);
                                const_expr->double_value_ = float_value;
                                values_row->emplace_back(const_expr);
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::array: {
                                SizeT dimension = value.size();
                                if (dimension == 0) {
                                    json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                    json_response["error_message"] = fmt::format("Empty embedding data: {}", value);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                auto first_elem = value[0];
                                auto first_elem_type = first_elem.type();
                                if (first_elem_type == nlohmann::json::value_t::number_integer or
                                    first_elem_type == nlohmann::json::value_t::number_unsigned) {

                                    // Generate constant expression
                                    infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kIntegerArray);
                                    DeferFn defer_free_integer_array([&]() {
                                        if (const_expr != nullptr) {
                                            delete const_expr;
                                            const_expr = nullptr;
                                        }
                                    });

                                    column_type_map.emplace(key, LiteralType::kIntegerArray);

                                    for (SizeT idx = 0; idx < dimension; ++idx) {
                                        const auto &value_ref = value[idx];
                                        const auto &value_type = value_ref.type();

                                        switch (value_type) {
                                            case nlohmann::json::value_t::number_integer: {
                                                const_expr->long_array_.emplace_back(value_ref.template get<i64>());
                                                break;
                                            }
                                            case nlohmann::json::value_t::number_unsigned: {
                                                const_expr->long_array_.emplace_back(value_ref.template get<u64>());
                                                break;
                                            }
                                            default: {
                                                json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                                json_response["error_message"] = fmt::format("Embedding element type should be integer");
                                                return ResponseFactory::createResponse(http_status, json_response.dump());
                                            }
                                        }
                                    }

                                    values_row->emplace_back(const_expr);
                                    const_expr = nullptr;

                                } else if (first_elem_type == nlohmann::json::value_t::number_float) {

                                    // Generate constant expression
                                    infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kDoubleArray);
                                    DeferFn defer_free_double_array([&]() {
                                        if (const_expr != nullptr) {
                                            delete const_expr;
                                            const_expr = nullptr;
                                        }
                                    });

                                    column_type_map.emplace(key, LiteralType::kDoubleArray);

                                    for (SizeT idx = 0; idx < dimension; ++idx) {
                                        const auto &value_ref = value[idx];
                                        const auto &value_type = value_ref.type();
                                        if (value_type != nlohmann::json::value_t::number_float) {
                                            json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                            json_response["error_message"] = fmt::format("Embedding element type should be float");
                                            return ResponseFactory::createResponse(http_status, json_response.dump());
                                        }

                                        const_expr->double_array_.emplace_back(value_ref.template get<double>());
                                    }

                                    values_row->emplace_back(const_expr);
                                    const_expr = nullptr;

                                } else {
                                    json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                    json_response["error_message"] = fmt::format("Embedding element type can only be integer or float");
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                break;
                            }
                            case nlohmann::json::value_t::string: {
                                auto string_value = value.template get<String>();
                                column_type_map.emplace(key, LiteralType::kString);

                                UniquePtr<ExpressionParserResult> expr_parsed_result = MakeUnique<ExpressionParserResult>();
                                ExprParser expr_parser;
                                expr_parser.Parse(string_value, expr_parsed_result.get());
                                if (expr_parsed_result->IsError() || expr_parsed_result->exprs_ptr_->size() != 1) {
                                    json_response["error_code"] = ErrorCode::kInvalidExpression;
                                    json_response["error_message"] = fmt::format("Invalid expression: {}", string_value);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                values_row->emplace_back(expr_parsed_result->exprs_ptr_->at(0));
                                expr_parsed_result->exprs_ptr_->at(0) = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::object:
                            case nlohmann::json::value_t::binary:
                            case nlohmann::json::value_t::null:
                            case nlohmann::json::value_t::discarded: {
                                json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                json_response["error_message"] = fmt::format("Embedding element type can only be integer or float");
                                return ResponseFactory::createResponse(http_status, json_response.dump());
                            }
                        }

                        //                    std::cout << key << " " << value.is_string() << std::endl;
                    }
                    column_values->emplace_back(values_row);
                    values_row = nullptr;
                }

                // Other rows except the first
                for (SizeT row_id = 1; row_id < row_count; ++row_id) {
                    const auto &row_json = http_body_json[row_id];

                    Vector<ParsedExpr *> *values_row = new Vector<infinity::ParsedExpr *>();
                    DeferFn defer_free_value_row([&]() {
                        if (values_row != nullptr) {
                            for (auto &value_ptr : *values_row) {
                                delete value_ptr;
                                value_ptr = nullptr;
                            }
                            delete values_row;
                            values_row = nullptr;
                        }
                    });
                    values_row->resize(column_count);

                    for (const auto &item : row_json.items()) {
                        const auto &key = item.key();
                        auto type_iter = column_type_map.find(key);
                        auto id_iter = column_name_id_map.find(key);
                        if (type_iter == column_type_map.end() || id_iter == column_name_id_map.end()) {
                            json_response["error_code"] = ErrorCode::kColumnNotExist;
                            json_response["error_message"] = fmt::format("Not existed column name: {}", key);
                            return ResponseFactory::createResponse(http_status, json_response.dump());
                        }

                        u64 column_id = id_iter->second;
                        const auto &value = item.value();
                        switch (value.type()) {
                            case nlohmann::json::value_t::boolean: {
                                if (type_iter->second != LiteralType::kBoolean) {
                                    json_response["error_code"] = ErrorCode::kDataTypeMismatch;
                                    json_response["error_message"] = fmt::format("Column: {} expect type BOOL", key);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kBoolean);
                                const_expr->bool_value_ = value.template get<bool>();
                                (*values_row)[column_id] = const_expr;
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::number_integer: {
                                if (type_iter->second != LiteralType::kInteger) {
                                    json_response["error_code"] = ErrorCode::kDataTypeMismatch;
                                    json_response["error_message"] = fmt::format("Column: {} expect type INTEGER", key);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kInteger);
                                const_expr->integer_value_ = value.template get<i64>();
                                (*values_row)[column_id] = const_expr;
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::number_unsigned: {
                                if (type_iter->second != LiteralType::kInteger) {
                                    json_response["error_code"] = ErrorCode::kDataTypeMismatch;
                                    json_response["error_message"] = fmt::format("Column: {} expect type INTEGER", key);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kInteger);
                                const_expr->integer_value_ = value.template get<u64>();
                                (*values_row)[column_id] = const_expr;
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::number_float: {
                                if (type_iter->second != LiteralType::kDouble) {
                                    json_response["error_code"] = ErrorCode::kDataTypeMismatch;
                                    json_response["error_message"] = fmt::format("Column: {} expect type FLOAT", key);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                // Generate constant expression
                                infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kDouble);
                                const_expr->double_value_ = value.template get<f64>();
                                (*values_row)[column_id] = const_expr;
                                const_expr = nullptr;
                                break;
                            }
                            case nlohmann::json::value_t::array: {
                                SizeT dimension = value.size();
                                if (dimension == 0) {
                                    json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                    json_response["error_message"] = fmt::format("Empty embedding data: {}", value);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                auto first_elem = value[0];
                                auto first_elem_type = first_elem.type();
                                if (first_elem_type == nlohmann::json::value_t::number_integer or
                                    first_elem_type == nlohmann::json::value_t::number_unsigned) {

                                    // Generate constant expression
                                    infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kIntegerArray);
                                    DeferFn defer_free_integer_array([&]() {
                                        if (const_expr != nullptr) {
                                            delete const_expr;
                                            const_expr = nullptr;
                                        }
                                    });

                                    for (SizeT idx = 0; idx < dimension; ++idx) {
                                        const auto &value_ref = value[idx];
                                        const auto &value_type = value_ref.type();

                                        switch (value_type) {
                                            case nlohmann::json::value_t::number_integer: {
                                                const_expr->long_array_.emplace_back(value.template get<i64>());
                                                break;
                                            }
                                            case nlohmann::json::value_t::number_unsigned: {
                                                const_expr->long_array_.emplace_back(value.template get<u64>());
                                                break;
                                            }
                                            default: {
                                                json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                                json_response["error_message"] = fmt::format("Embedding element type should be integer");
                                                return ResponseFactory::createResponse(http_status, json_response.dump());
                                            }
                                        }
                                    }

                                    (*values_row)[column_id] = const_expr;
                                    const_expr = nullptr;
                                } else if (first_elem_type == nlohmann::json::value_t::number_float) {

                                    // Generate constant expression
                                    infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kDoubleArray);
                                    DeferFn defer_free_double_array([&]() {
                                        if (const_expr != nullptr) {
                                            delete const_expr;
                                            const_expr = nullptr;
                                        }
                                    });

                                    for (SizeT idx = 0; idx < dimension; ++idx) {
                                        const auto &value_ref = value[idx];
                                        const auto &value_type = value_ref.type();
                                        if (value_type != nlohmann::json::value_t::number_float) {
                                            json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                            json_response["error_message"] = fmt::format("Embedding element type should be float");
                                            return ResponseFactory::createResponse(http_status, json_response.dump());
                                        }

                                        const_expr->double_array_.emplace_back(value.template get<double>());
                                    }

                                    (*values_row)[column_id] = const_expr;
                                    const_expr = nullptr;
                                } else {
                                    json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                    json_response["error_message"] = fmt::format("Embedding element type can only be integer or float");
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                break;
                            }
                            case nlohmann::json::value_t::string: {
                                if (type_iter->second != LiteralType::kString) {
                                    json_response["error_code"] = ErrorCode::kDataTypeMismatch;
                                    json_response["error_message"] = fmt::format("Column: {} expect type STRING", key);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                auto string_value = value.template get<String>();

                                UniquePtr<ExpressionParserResult> expr_parsed_result = MakeUnique<ExpressionParserResult>();
                                ExprParser expr_parser;
                                expr_parser.Parse(string_value, expr_parsed_result.get());
                                if (expr_parsed_result->IsError() || expr_parsed_result->exprs_ptr_->size() != 1) {
                                    json_response["error_code"] = ErrorCode::kInvalidExpression;
                                    json_response["error_message"] = fmt::format("Invalid expression: {}", string_value);
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                (*values_row)[column_id] = expr_parsed_result->exprs_ptr_->at(0);
                                expr_parsed_result->exprs_ptr_->at(0) = nullptr;

                                break;
                            }
                            case nlohmann::json::value_t::object:
                            case nlohmann::json::value_t::binary:
                            case nlohmann::json::value_t::null:
                            case nlohmann::json::value_t::discarded: {
                                json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                json_response["error_message"] = fmt::format("Embedding element type can only be integer or float");
                                return ResponseFactory::createResponse(http_status, json_response.dump());
                            }
                        }
                    }
                    column_values->emplace_back(values_row);
                    values_row = nullptr;
                }

                auto database_name = request->getPathVariable("database_name");
                auto table_name = request->getPathVariable("table_name");
                auto result = infinity->Insert(database_name, table_name, columns, column_values);
                columns = nullptr;
                column_values = nullptr;
                json_response["error_code"] = 0;
                http_status = HTTPStatus::CODE_200;

            } else {
                json_response["error_code"] = ErrorCode::kInvalidJsonFormat;
                json_response["error_message"] = fmt::format("Invalid json format: {}", data_body);
            }

        } catch (nlohmann::json::exception &e) {
            json_response["error_code"] = ErrorCode::kInvalidJsonFormat;
            json_response["error_message"] = e.what();
        }

        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class DeleteHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        nlohmann::json json_response;
        HTTPStatus http_status = HTTPStatus::CODE_500;

        String data_body = request->readBodyToString();
        try {
            nlohmann::json http_body_json = nlohmann::json::parse(data_body);

            const String filter_string = http_body_json["filter"];

            UniquePtr<ExpressionParserResult> expr_parsed_result = MakeUnique<ExpressionParserResult>();
            ExprParser expr_parser;
            expr_parser.Parse(filter_string, expr_parsed_result.get());
            if (expr_parsed_result->IsError() || expr_parsed_result->exprs_ptr_->size() != 1) {
                json_response["error_code"] = ErrorCode::kInvalidFilterExpression;
                json_response["error_message"] = fmt::format("Invalid filter expression: {}", filter_string);
                return ResponseFactory::createResponse(http_status, json_response.dump());
            }

            auto database_name = request->getPathVariable("database_name");
            auto table_name = request->getPathVariable("table_name");
            const QueryResult result = infinity->Delete(database_name, table_name, expr_parsed_result->exprs_ptr_->at(0));
            expr_parsed_result->exprs_ptr_->at(0) = nullptr;

            // Only one block
            DataBlock *data_block = result.result_table_->GetDataBlockById(0).get();

            // Get sum delete rows
            Value value = data_block->GetValue(1, 0);
            json_response["delete_row_count"] = value.value_.big_int;
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;

        } catch (nlohmann::json::exception &e) {
            json_response["error_code"] = ErrorCode::kInvalidJsonFormat;
            json_response["error_message"] = e.what();
        }

        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class UpdateHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        nlohmann::json json_response;
        HTTPStatus http_status = HTTPStatus::CODE_500;

        String data_body = request->readBodyToString();
        try {
            nlohmann::json http_body_json = nlohmann::json::parse(data_body);
            const auto &update_clause = http_body_json["update"];

            Vector<UpdateExpr *> *update_expr_array = new Vector<UpdateExpr *>();
            DeferFn defer_free_update_expr_array([&]() {
                if(update_expr_array != nullptr) {
                    for (auto &expr : *update_expr_array) {
                        delete expr;
                    }
                    delete update_expr_array;
                    update_expr_array = nullptr;
                }
            });
            update_expr_array->reserve(update_clause.size());

            for (const auto &update_elem : update_clause.items()) {
                UpdateExpr *update_expr = new UpdateExpr();
                DeferFn defer_free_update_expr([&]() {
                    if (update_expr != nullptr) {
                        delete update_expr;
                        update_expr = nullptr;
                    }
                });
                update_expr->column_name = update_elem.key();
                const auto &value = update_elem.value();
                switch (value.type()) {
                    case nlohmann::json::value_t::boolean: {
                        // Generate constant expression
                        infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kBoolean);
                        const_expr->bool_value_ = value.template get<bool>();
                        update_expr->value = const_expr;
                        const_expr = nullptr;
                        break;
                    }
                    case nlohmann::json::value_t::number_integer: {
                        // Generate constant expression
                        infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kInteger);
                        const_expr->integer_value_ = value.template get<i64>();
                        update_expr->value = const_expr;
                        const_expr = nullptr;
                        break;
                    }
                    case nlohmann::json::value_t::number_unsigned: {
                        // Generate constant expression
                        infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kInteger);
                        const_expr->integer_value_ = value.template get<u64>();
                        update_expr->value = const_expr;
                        const_expr = nullptr;
                        break;
                    }
                    case nlohmann::json::value_t::number_float: {
                        // Generate constant expression
                        infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kDouble);
                        const_expr->double_value_ = value.template get<f64>();
                        update_expr->value = const_expr;
                        const_expr = nullptr;
                        break;
                    }
                    case nlohmann::json::value_t::string: {
                        infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kString);
                        auto str_value = value.template get<std::string>();
                        const_expr->str_value_ = new char[str_value.size() + 1]{0};
                        memcpy(const_expr->str_value_, str_value.c_str(), str_value.size());
                        update_expr->value = const_expr;
                        const_expr = nullptr;
                        break;
                    }
                    case nlohmann::json::value_t::array: {
                        SizeT dimension = value.size();
                        if (dimension == 0) {
                            json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                            json_response["error_message"] = fmt::format("Empty embedding data: {}", value);
                            return ResponseFactory::createResponse(http_status, json_response.dump());
                        }

                        auto first_elem = value[0];
                        auto first_elem_type = first_elem.type();
                        if (first_elem_type == nlohmann::json::value_t::number_integer or
                            first_elem_type == nlohmann::json::value_t::number_unsigned) {

                            // Generate constant expression
                            infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kIntegerArray);
                            DeferFn defer_free_integer_array([&]() {
                                if (const_expr != nullptr) {
                                    delete const_expr;
                                    const_expr = nullptr;
                                }
                            });

                            for (SizeT idx = 0; idx < dimension; ++idx) {
                                const auto &value_ref = value[idx];
                                const auto &value_type = value_ref.type();

                                switch (value_type) {
                                    case nlohmann::json::value_t::number_integer: {
                                        const_expr->long_array_.emplace_back(value.template get<i64>());
                                        break;
                                    }
                                    case nlohmann::json::value_t::number_unsigned: {
                                        const_expr->long_array_.emplace_back(value.template get<u64>());
                                        break;
                                    }
                                    default: {
                                        json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                        json_response["error_message"] = fmt::format("Embedding element type should be integer");
                                        return ResponseFactory::createResponse(http_status, json_response.dump());
                                    }
                                }
                            }

                            update_expr->value = const_expr;
                            const_expr = nullptr;
                        } else if (first_elem_type == nlohmann::json::value_t::number_float) {

                            // Generate constant expression
                            infinity::ConstantExpr *const_expr = new ConstantExpr(LiteralType::kDoubleArray);
                            DeferFn defer_free_double_array([&]() {
                                if (const_expr != nullptr) {
                                    delete const_expr;
                                    const_expr = nullptr;
                                }
                            });

                            for (SizeT idx = 0; idx < dimension; ++idx) {
                                const auto &value_ref = value[idx];
                                const auto &value_type = value_ref.type();
                                if (value_type != nlohmann::json::value_t::number_float) {
                                    json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                                    json_response["error_message"] = fmt::format("Embedding element type should be float");
                                    return ResponseFactory::createResponse(http_status, json_response.dump());
                                }

                                const_expr->double_array_.emplace_back(value.template get<double>());
                            }

                            update_expr->value = const_expr;
                            const_expr = nullptr;
                        } else {
                            json_response["error_code"] = ErrorCode::kInvalidEmbeddingDataType;
                            json_response["error_message"] = fmt::format("Embedding element type can only be integer or float");
                            return ResponseFactory::createResponse(http_status, json_response.dump());
                        }
                        break;
                    }

                    case nlohmann::json::value_t::object:
                    case nlohmann::json::value_t::binary:
                    case nlohmann::json::value_t::null:
                    case nlohmann::json::value_t::discarded: {
                        json_response["error_code"] = ErrorCode::kInvalidExpression;
                        json_response["error_message"] = fmt::format("Invalid update set expression");
                        return ResponseFactory::createResponse(http_status, json_response.dump());
                    }
                }

                update_expr_array->emplace_back(update_expr);
                update_expr = nullptr;
            }

            String where_clause = http_body_json["filter"];

            UniquePtr<ExpressionParserResult> expr_parsed_result = MakeUnique<ExpressionParserResult>();
            ExprParser expr_parser;
            expr_parser.Parse(where_clause, expr_parsed_result.get());
            if (expr_parsed_result->IsError() || expr_parsed_result->exprs_ptr_->size() != 1) {
                json_response["error_code"] = ErrorCode::kInvalidFilterExpression;
                json_response["error_message"] = fmt::format("Invalid filter expression: {}", where_clause);
                return ResponseFactory::createResponse(http_status, json_response.dump());
            }

            auto database_name = request->getPathVariable("database_name");
            auto table_name = request->getPathVariable("table_name");

            const QueryResult result = infinity->Update(database_name, table_name, expr_parsed_result->exprs_ptr_->at(0), update_expr_array);
            expr_parsed_result->exprs_ptr_->at(0) = nullptr;
            update_expr_array = nullptr;

            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;

        } catch (nlohmann::json::exception &e) {
            json_response["error_code"] = ErrorCode::kInvalidJsonFormat;
            json_response["error_message"] = e.what();
        }

        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class SelectHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        String data_body = request->readBodyToString();

        nlohmann::json json_response;
        HTTPStatus http_status;

        HTTPSearch::Process(infinity.get(), database_name, table_name, data_body, http_status, json_response);

        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ListTableIndexesHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto result = infinity->ListTableIndexes(database_name, table_name);

        HTTPStatus http_status;
        nlohmann::json json_response;

        if (result.IsOk()) {

            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();

                for (int row = 0; row < row_count; ++row) {
                    nlohmann::json json_index;

                    {
                        // index name
                        Value value = data_block->GetValue(0, row);
                        const String &column_value = value.ToString();
                        json_index["index_name"] = column_value;
                    }

                    {
                        // index type
                        Value value = data_block->GetValue(1, row);
                        const String &column_value = value.ToString();
                        json_index["index_type"] = column_value;
                    }

                    {
                        // columns
                        Value value = data_block->GetValue(3, row);
                        const String &column_value = value.ToString();
                        json_index["columns"] = column_value;
                    }

                    json_response["indexes"].push_back(json_index);
                }
            }
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowVariableHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto variable_name = request->getPathVariable("variable_name");
        auto result = infinity->ShowVariable(variable_name);

        nlohmann::json json_response;
        HTTPStatus http_status;

        if (result.IsOk()) {
            json_response["error_code"] = 0;
            json_response["variable_name"] = variable_name;
            DataBlock *data_block = result.result_table_->GetDataBlockById(0).get();
            Value value = data_block->GetValue(0, 0);
            const String &variable_value = value.ToString();
            json_response["variable_value"] = variable_value;

            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowTableIndexDetailHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto index_name = request->getPathVariable("index_name");

        auto result = infinity->ShowIndex(database_name, table_name, index_name);

        HTTPStatus http_status;
        nlohmann::json json_response;

        if (result.IsOk()) {

            SizeT block_rows = result.result_table_->DataBlockCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                SharedPtr<DataBlock> data_block = result.result_table_->GetDataBlockById(block_id);
                auto row_count = data_block->row_count();
                for (int row = 0; row < row_count; ++row) {
                    auto field_name = data_block->GetValue(0, row).ToString();
                    auto field_value = data_block->GetValue(1, row).ToString();
                    json_response[field_name] = field_value;
                }
            }

            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class DropIndexHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto index_name = request->getPathVariable("index_name");
        auto result = infinity->DropIndex(database_name, table_name, index_name, DropIndexOptions());

        nlohmann::json json_response;
        HTTPStatus http_status;
        if (result.IsOk()) {
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class CreateIndexHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto index_name = request->getPathVariable("index_name");

        String body_info_str = request->readBodyToString();
        nlohmann::json body_info_json = nlohmann::json::parse(body_info_str);

        CreateIndexOptions options;
        auto create_option = body_info_json["create_option"];
        auto ignore_if_exists = create_option["ignore_if_exists"];
        if (ignore_if_exists.is_boolean() && ignore_if_exists) {
            options.conflict_type_ = ConflictType::kIgnore;
        }

        auto fields = body_info_json["fields"];
        auto index = body_info_json["index"];

        auto index_info_list = new Vector<IndexInfo *>();
        {
            auto index_info = new IndexInfo();
            index_info->column_name_ = fields[0];
            auto index_param_list = new Vector<InitParameter *>();

            for (auto &ele : index.items()) {
                String name = ele.key();
                auto value = ele.value();
                if (!ele.value().is_string()) {
                    value = ele.value().dump();
                }

                if (strcmp(name.c_str(), "type") == 0) {
                    index_info->index_type_ = IndexInfo::StringToIndexType(value);
                } else {
                    index_param_list->push_back(new InitParameter(name, value));
                }
            }

            index_info->index_param_list_ = index_param_list;
            index_info_list->push_back(index_info);
        }

        auto result = infinity->CreateIndex(database_name, table_name, index_name, index_info_list, options);

        nlohmann::json json_response;
        HTTPStatus http_status;
        if (result.IsOk()) {
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowSegmentDetailHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto segment_id = std::atoi(request->getPathVariable("segment_id").get()->c_str());
        auto result = infinity->ShowSegment(database_name, table_name, segment_id);

        HTTPStatus http_status;
        nlohmann::json json_response;

        if (result.IsOk()) {

            SizeT block_rows = result.result_table_->DataBlockCount();
            auto column_cnt = result.result_table_->ColumnCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                for (int row = 0; row < row_count; ++row) {
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_response[column_name] = column_value;
                    }
                }
            }
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowSegmentsListHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto result = infinity->ShowSegments(database_name, table_name);

        HTTPStatus http_status;
        nlohmann::json json_response;

        if (result.IsOk()) {

            SizeT block_rows = result.result_table_->DataBlockCount();
            auto column_cnt = result.result_table_->ColumnCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                for (int row = 0; row < row_count; ++row) {

                    nlohmann::json json_segment;
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_segment[column_name] = column_value;
                    }
                    json_response["segments"].push_back(json_segment);

                }
            }
            json_response["table_name"] = table_name;
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowBlocksListHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto segment_id = std::atoi(request->getPathVariable("segment_id").get()->c_str());
        auto result = infinity->ShowBlocks(database_name, table_name, segment_id);

        HTTPStatus http_status;
        nlohmann::json json_response;

        if (result.IsOk()) {

            SizeT block_rows = result.result_table_->DataBlockCount();
            auto column_cnt = result.result_table_->ColumnCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                for (int row = 0; row < row_count; ++row) {

                    nlohmann::json json_block;
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_block[column_name] = column_value;
                    }
                    json_response["blocks"].push_back(json_block);

                }
            }

            json_response["segment_id"] = segment_id;
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

class ShowBlockDetailHandler final : public HttpRequestHandler {
public:
    SharedPtr<OutgoingResponse> handle(const SharedPtr<IncomingRequest> &request) final {
        auto infinity = Infinity::RemoteConnect();
        DeferFn defer_fn([&]() { infinity->RemoteDisconnect(); });

        auto database_name = request->getPathVariable("database_name");
        auto table_name = request->getPathVariable("table_name");
        auto segment_id = std::atoi(request->getPathVariable("segment_id").get()->c_str());
        auto block_id = std::atoi(request->getPathVariable("block_id").get()->c_str());
        auto result = infinity->ShowBlock(database_name, table_name, segment_id, block_id);

        HTTPStatus http_status;
        nlohmann::json json_response;

        if (result.IsOk()) {
            SizeT block_rows = result.result_table_->DataBlockCount();
            auto column_cnt = result.result_table_->ColumnCount();
            for (SizeT block_id = 0; block_id < block_rows; ++block_id) {
                DataBlock *data_block = result.result_table_->GetDataBlockById(block_id).get();
                auto row_count = data_block->row_count();
                for (int row = 0; row < row_count; ++row) {
                    for (SizeT col = 0; col < column_cnt; ++col) {
                        const String &column_name = result.result_table_->GetColumnNameById(col);
                        Value value = data_block->GetValue(col, row);
                        const String &column_value = value.ToString();
                        json_response[column_name] = column_value;
                    }
                }
            }
            json_response["error_code"] = 0;
            http_status = HTTPStatus::CODE_200;
        } else {
            json_response["error_code"] = result.ErrorCode();
            json_response["error_message"] = result.ErrorMsg();
            http_status = HTTPStatus::CODE_500;
        }
        return ResponseFactory::createResponse(http_status, json_response.dump());
    }
};

} // namespace

namespace infinity {

void HTTPServer::Start(u16 port) {

    WebEnvironment::init();

    SharedPtr<HttpRouter> router = HttpRouter::createShared();

    // database
    router->route("GET", "/databases", MakeShared<ListDatabaseHandler>());
    router->route("POST", "/databases/{database_name}", MakeShared<CreateDatabaseHandler>());
    router->route("DELETE", "/databases/{database_name}", MakeShared<DropDatabaseHandler>());
    router->route("GET", "/databases/{database_name}", MakeShared<ShowDatabaseHandler>());

    // table
    router->route("GET", "/databases/{database_name}/tables", MakeShared<ListTableHandler>());
    router->route("POST", "/databases/{database_name}/tables/{table_name}", MakeShared<CreateTableHandler>());
    router->route("DELETE", "/databases/{database_name}/tables/{table_name}", MakeShared<DropTableHandler>());
    router->route("GET", "/databases/{database_name}/tables/{table_name}", MakeShared<ShowTableHandler>());
    router->route("GET", "/databases/{database_name}/tables/{table_name}/columns", MakeShared<ShowTableColumnsHandler>());

    // DML
    router->route("POST", "/databases/{database_name}/tables/{table_name}/docs", MakeShared<InsertHandler>());
    router->route("DELETE", "/databases/{database_name}/tables/{table_name}/docs", MakeShared<DeleteHandler>());
    router->route("PUT", "/databases/{database_name}/tables/{table_name}/docs", MakeShared<UpdateHandler>());

    // DQL
    router->route("GET", "/databases/{database_name}/tables/{table_name}/docs", MakeShared<SelectHandler>());

    // index
    router->route("GET", "/databases/{database_name}/tables/{table_name}/indexes", MakeShared<ListTableIndexesHandler>());
    router->route("GET", "/databases/{database_name}/tables/{table_name}/indexes/{index_name}", MakeShared<ShowTableIndexDetailHandler>());
    router->route("DELETE", "/databases/{database_name}/tables/{table_name}/indexes/{index_name}", MakeShared<DropIndexHandler>());
    router->route("POST", "/databases/{database_name}/tables/{table_name}/indexes/{index_name}", MakeShared<CreateIndexHandler>());

    // segment
    router->route("GET", "/databases/{database_name}/tables/{table_name}/segments/{segment_id}", MakeShared<ShowSegmentDetailHandler>());
    router->route("GET", "/databases/{database_name}/tables/{table_name}/segments", MakeShared<ShowSegmentsListHandler>());

    // block
    router->route("GET", "/databases/{database_name}/tables/{table_name}/segments/{segment_id}/blocks/{block_id}", MakeShared<ShowBlockDetailHandler>());
    router->route("GET", "/databases/{database_name}/tables/{table_name}/segments/{segment_id}/blocks", MakeShared<ShowBlocksListHandler>());

    // variable
    router->route("GET", "/variables/{variable_name}", MakeShared<ShowVariableHandler>());

    SharedPtr<HttpConnectionProvider> connection_provider = HttpConnectionProvider::createShared({"localhost", port, WebAddress::IP_4});
    SharedPtr<HttpConnectionHandler> connection_handler = HttpConnectionHandler::createShared(router);

    server_ = MakeShared<WebServer>(connection_provider, connection_handler);

    fmt::print("HTTP server listen on port: {}\n", port);

    server_->run();
}

void HTTPServer::Shutdown() {

    server_->stop();
    WebEnvironment::destroy();
}

} // namespace infinity
