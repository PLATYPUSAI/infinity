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

module;

#include <iostream>

module infinity;

import stl;
import logger;
import config;
import resource_manager;
import task_scheduler;
import storage;
import local_file_system;
import third_party;
import query_options;
import query_result;
import infinity_context;
import session;
import session_manager;
import query_context;
import parsed_expr;
import search_expr;

import status;
import create_statement;
import show_statement;
import drop_statement;
import command_statement;
import select_statement;
import flush_statement;
import table_reference;
import insert_statement;
import copy_statement;
import delete_statement;
import optimize_statement;

import create_schema_info;
import drop_schema_info;
import create_table_info;
import drop_table_info;
import query_options;
import extra_ddl_info;
import drop_index_info;
import drop_table_info;

namespace infinity {

u64 Infinity::GetSessionId() { return session_->session_id(); }

void Infinity::LocalInit(const String &path) {
    LocalFileSystem fs;
    if (!fs.Exists(path)) {
        fs.CreateDirectory(path);
    }

    SharedPtr<String> config_path = MakeShared<String>(path + "/infinity_conf.toml");

    InfinityContext::instance().Init(config_path);
}

void Infinity::LocalUnInit() { InfinityContext::instance().UnInit(); }

SharedPtr<Infinity> Infinity::LocalConnect() {
    SharedPtr<Infinity> infinity_ptr = MakeShared<Infinity>();

    SessionManager *session_mgr = InfinityContext::instance().session_manager();
    infinity_ptr->session_ = session_mgr->CreateLocalSession();
    return infinity_ptr;
}

void Infinity::LocalDisconnect() {
    //    fmt::print("To disconnect the database.\n");
}

SharedPtr<Infinity> Infinity::RemoteConnect() {
    SharedPtr<Infinity> infinity_ptr = MakeShared<Infinity>();
    SessionManager *session_mgr = InfinityContext::instance().session_manager();
    infinity_ptr->session_ = session_mgr->CreateRemoteSession();
    return infinity_ptr;
}

void Infinity::RemoteDisconnect() {
    SessionManager *session_mgr = InfinityContext::instance().session_manager();
    session_mgr->RemoveSessionByID(session_->session_id());
    session_.reset();
}

QueryResult Infinity::CreateDatabase(const String &db_name, const CreateDatabaseOptions &create_db_options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<CreateStatement> create_statement = MakeUnique<CreateStatement>();
    SharedPtr<CreateSchemaInfo> create_schema_info = MakeShared<CreateSchemaInfo>();
    create_schema_info->schema_name_ = db_name;
    create_statement->create_info_ = create_schema_info;

    create_statement->create_info_->conflict_type_ = create_db_options.conflict_type_;
    QueryResult query_result = query_context_ptr->QueryStatement(create_statement.get());
    return query_result;
}

QueryResult Infinity::DropDatabase(const String &db_name, const DropDatabaseOptions &drop_database_options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<DropStatement> drop_statement = MakeUnique<DropStatement>();
    SharedPtr<DropSchemaInfo> drop_schema_info = MakeShared<DropSchemaInfo>();
    drop_schema_info->schema_name_ = db_name;
    drop_statement->drop_info_ = drop_schema_info;
    drop_statement->drop_info_->conflict_type_ = drop_database_options.conflict_type_;
    QueryResult result = query_context_ptr->QueryStatement(drop_statement.get());
    return result;
}

QueryResult Infinity::ListDatabases() {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->show_type_ = ShowStmtType::kDatabases;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::GetDatabase(const String &db_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<CommandStatement> command_statement = MakeUnique<CommandStatement>();
    command_statement->command_info_ = MakeUnique<UseCmd>(db_name.c_str());
    QueryResult result = query_context_ptr->QueryStatement(command_statement.get());
    return result;
}

QueryResult Infinity::ShowDatabase(const String &db_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->show_type_ = ShowStmtType::kDatabase;
    show_statement->schema_name_ = db_name;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::Query(const String &query_text) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    QueryResult result = query_context_ptr->Query(query_text);
    return result;
}

QueryResult Infinity::Flush() {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<FlushStatement> flush_statement = MakeUnique<FlushStatement>();
    flush_statement->type_ = FlushType::kData;

    QueryResult result = query_context_ptr->QueryStatement(flush_statement.get());
    return result;
}

QueryResult Infinity::ShowVariable(const String &variable_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());

    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->var_name_ = variable_name;
    show_statement->show_type_ = ShowStmtType::kVar;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::CreateTable(const String &db_name,
                                  const String &table_name,
                                  Vector<ColumnDef *> column_defs,
                                  Vector<TableConstraint *> constraints,
                                  const CreateTableOptions &create_table_options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<CreateStatement> create_statement = MakeUnique<CreateStatement>();
    SharedPtr<CreateTableInfo> create_table_info = MakeShared<CreateTableInfo>();
    create_table_info->schema_name_ = db_name;
    create_table_info->table_name_ = table_name;
    create_table_info->column_defs_ = std::move(column_defs);
    create_table_info->constraints_ = std::move(constraints);
    create_table_info->conflict_type_ = create_table_options.conflict_type_;
    create_table_info->properties_ = create_table_options.properties_;
    create_statement->create_info_ = std::move(create_table_info);
    QueryResult result = query_context_ptr->QueryStatement(create_statement.get());
    return result;
}

QueryResult Infinity::DropTable(const String &db_name, const String &table_name, const DropTableOptions &options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<DropStatement> drop_statement = MakeUnique<DropStatement>();
    SharedPtr<DropTableInfo> drop_table_info = MakeShared<DropTableInfo>();
    drop_table_info->schema_name_ = db_name;
    drop_table_info->table_name_ = table_name;
    drop_table_info->conflict_type_ = options.conflict_type_;
    drop_statement->drop_info_ = drop_table_info;
    QueryResult result = query_context_ptr->QueryStatement(drop_statement.get());
    return result;
}

QueryResult Infinity::ListTables(const String &db_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->set_current_schema(db_name);
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->show_type_ = ShowStmtType::kTables;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowTable(const String &db_name, const String &table_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->show_type_ = ShowStmtType::kTable;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowColumns(const String &db_name, const String &table_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->show_type_ = ShowStmtType::kColumns;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowTables(const String &db_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->show_type_ = ShowStmtType::kTables;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::GetTable(const String &db_name, const String &table_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->set_current_schema(db_name);
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<CommandStatement> command_statement = MakeUnique<CommandStatement>();
    command_statement->command_info_ = MakeUnique<CheckTable>(table_name.c_str());
    QueryResult result = query_context_ptr->QueryStatement(command_statement.get());
    return result;
}

QueryResult Infinity::ListTableIndexes(const String &db_name, const String &table_name){
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->show_type_ = ShowStmtType::kIndexes;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;

}

QueryResult Infinity::CreateIndex(const String &db_name,
                                  const String &table_name,
                                  const String &index_name,
                                  Vector<IndexInfo *> *index_info_list,
                                  const CreateIndexOptions &create_index_options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());

    UniquePtr<CreateStatement> create_statement = MakeUnique<CreateStatement>();
    SharedPtr<CreateIndexInfo> create_index_info = MakeShared<CreateIndexInfo>();

    create_index_info->schema_name_ = db_name;
    create_index_info->table_name_ = table_name;
    create_index_info->index_name_ = index_name;
    create_index_info->index_info_list_ = index_info_list;

    create_statement->create_info_ = create_index_info;
    create_statement->create_info_->conflict_type_ = create_index_options.conflict_type_;

    QueryResult result = query_context_ptr->QueryStatement(create_statement.get());
    return result;
}

QueryResult
Infinity::DropIndex(const String &db_name, const String &table_name, const String &index_name, const DropIndexOptions &drop_index_options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<DropStatement> drop_statement = MakeUnique<DropStatement>();
    SharedPtr<DropIndexInfo> drop_index_info = MakeShared<DropIndexInfo>();

    drop_index_info->schema_name_ = db_name;
    drop_index_info->table_name_ = table_name;
    drop_index_info->index_name_ = index_name;
    drop_statement->drop_info_ = drop_index_info;

    drop_statement->drop_info_->conflict_type_ = drop_index_options.conflict_type_;

    QueryResult result = query_context_ptr->QueryStatement(drop_statement.get());
    return result;
}

QueryResult Infinity::ShowIndex(const String &db_name, const String &table_name, const String &index_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->index_name_ = index_name;
    show_statement->show_type_ = ShowStmtType::kIndex;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowSegment(const String &db_name,const String &table_name, const SegmentID &segment_id) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->segment_id_ = segment_id;
    show_statement->show_type_ = ShowStmtType::kSegment;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowSegments(const String &db_name,const String &table_name) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->show_type_ = ShowStmtType::kSegments;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowBlock(const String &db_name,const String &table_name, const SegmentID &segment_id, const BlockID &block_id) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->segment_id_ = segment_id;
    show_statement->block_id_ = block_id;
    show_statement->show_type_ = ShowStmtType::kBlock;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::ShowBlocks(const String &db_name,const String &table_name, const SegmentID &segment_id) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ShowStatement> show_statement = MakeUnique<ShowStatement>();
    show_statement->schema_name_ = db_name;
    show_statement->table_name_ = table_name;
    show_statement->segment_id_ = segment_id;
    show_statement->show_type_ = ShowStmtType::kBlocks;
    QueryResult result = query_context_ptr->QueryStatement(show_statement.get());
    return result;
}

QueryResult Infinity::Insert(const String &db_name, const String &table_name, Vector<String> *columns, Vector<Vector<ParsedExpr *> *> *values) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<InsertStatement> insert_statement = MakeUnique<InsertStatement>();

    insert_statement->schema_name_ = db_name;
    insert_statement->table_name_ = table_name;
    insert_statement->columns_ = columns;
    insert_statement->values_ = values;

    QueryResult result = query_context_ptr->QueryStatement(insert_statement.get());
    return result;
}

QueryResult Infinity::Import(const String &db_name, const String &table_name, const String &path, ImportOptions import_options) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<CopyStatement> import_statement = MakeUnique<CopyStatement>();

    import_statement->copy_from_ = true;
    import_statement->file_path_ = path;
    import_statement->schema_name_ = db_name;
    import_statement->table_name_ = table_name;

    import_statement->header_ = false;
    import_statement->copy_file_type_ = import_options.copy_file_type_;
    import_statement->delimiter_ = import_options.delimiter_;

    QueryResult result = query_context_ptr->QueryStatement(import_statement.get());
    return result;
}

QueryResult Infinity::Delete(const String &db_name, const String &table_name, ParsedExpr *filter) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<DeleteStatement> delete_statement = MakeUnique<DeleteStatement>();
    delete_statement->schema_name_ = db_name;
    delete_statement->table_name_ = table_name;
    delete_statement->where_expr_ = filter;
    QueryResult result = query_context_ptr->QueryStatement(delete_statement.get());
    return result;
}

QueryResult Infinity::Update(const String &db_name, const String &table_name, ParsedExpr *filter, Vector<UpdateExpr *> *update_list) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<UpdateStatement> update_statement = MakeUnique<UpdateStatement>();
    update_statement->schema_name_ = db_name;
    update_statement->table_name_ = table_name;
    update_statement->where_expr_ = filter;
    update_statement->update_expr_array_ = update_list;
    QueryResult result = query_context_ptr->QueryStatement(update_statement.get());
    return result;
}

QueryResult Infinity::Explain(const String &db_name,
                              const String &table_name,
                              ExplainType explain_type,
                              SearchExpr *search_expr,
                              ParsedExpr *filter,
                              Vector<ParsedExpr *> *output_columns) {

    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<ExplainStatement> explain_statment = MakeUnique<ExplainStatement>();
    explain_statment->type_ = explain_type;

    SelectStatement *select_statement = new SelectStatement();

    auto *table_ref = new TableReference();
    table_ref->db_name_ = db_name;
    table_ref->table_name_ = table_name;
    select_statement->table_ref_ = table_ref;
    select_statement->select_list_ = output_columns;
    select_statement->where_expr_ = filter;
    select_statement->search_expr_ = search_expr;

    explain_statment->statement_ = select_statement;

    QueryResult result = query_context_ptr->QueryStatement(explain_statment.get());
    return result;
}

QueryResult
Infinity::Search(const String &db_name, const String &table_name, SearchExpr *search_expr, ParsedExpr *filter, Vector<ParsedExpr *> *output_columns) {
    UniquePtr<QueryContext> query_context_ptr = MakeUnique<QueryContext>(session_.get());
    query_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<SelectStatement> select_statement = MakeUnique<SelectStatement>();

    auto *table_ref = new TableReference();
    table_ref->db_name_ = db_name;
    table_ref->table_name_ = table_name;
    select_statement->table_ref_ = table_ref;
    select_statement->select_list_ = output_columns;
    select_statement->where_expr_ = filter;
    select_statement->search_expr_ = search_expr;

    QueryResult result = query_context_ptr->QueryStatement(select_statement.get());
    return result;
}

QueryResult
Infinity::Optimize(const String &db_name, const String &table_name) {
    UniquePtr<QueryContext> optimize_context_ptr = MakeUnique<QueryContext>(session_.get());
    optimize_context_ptr->Init(InfinityContext::instance().config(),
                            InfinityContext::instance().task_scheduler(),
                            InfinityContext::instance().storage(),
                            InfinityContext::instance().resource_manager(),
                            InfinityContext::instance().session_manager());
    UniquePtr<OptimizeStatement> optimize_statement = MakeUnique<OptimizeStatement>();

    optimize_statement->schema_name_ = db_name;
    optimize_statement->table_name_ = table_name;

    QueryResult result = optimize_context_ptr->QueryStatement(optimize_statement.get());
    return result;
}

} // namespace infinity
