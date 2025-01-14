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

// #include <fstream>
#include <string>

module physical_command;

import stl;
import query_context;
import operator_state;

import profiler;
import local_file_system;
import file_writer;
import table_def;
import data_table;
import options;
import third_party;
import defer_op;
import config;
import status;
import infinity_exception;
import variables;
import logger;

namespace infinity {

void PhysicalCommand::Init() {}

bool PhysicalCommand::Execute(QueryContext *query_context, OperatorState *operator_state) {
    DeferFn defer_fn([&]() { operator_state->SetComplete(); });
    switch (command_info_->type()) {
        case CommandType::kUse: {
            UseCmd *use_command = (UseCmd *)(command_info_.get());
            query_context->set_current_schema(use_command->db_name());
            break;
        }
        case CommandType::kSet: {
            SetCmd *set_command = (SetCmd *)(command_info_.get());
            switch(set_command->scope()) {
                case SetScope::kSession: {
                    SessionVariable session_var = VarUtil::GetSessionVarByName(set_command->var_name());
                    switch(session_var) {
                        case SessionVariable::kEnableProfile: {
                            if (set_command->value_type() != SetVarType::kBool) {
                                Status status = Status::DataTypeMismatch("Boolean", set_command->value_type_str());
                                LOG_ERROR(status.message());
                                RecoverableError(status);
                            }
                            query_context->current_session()->SetProfile(set_command->value_bool());
                            return true;
                        }
                        case SessionVariable::kInvalid: {
                            Status status = Status::InvalidCommand(fmt::format("Unknown session variable: {}", set_command->var_name()));
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                        }
                        default: {
                            Status status = Status::InvalidCommand(fmt::format("Session variable: {} is read-only", set_command->var_name()));
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                        }
                    }
                    break;
                }
                case SetScope::kGlobal: {
                    GlobalVariable global_var = VarUtil::GetGlobalVarByName(set_command->var_name());
                    switch(global_var) {
                        case GlobalVariable::kProfileRecordCapacity: {
                            if (set_command->value_type() != SetVarType::kInteger) {
                                Status status = Status::DataTypeMismatch("Integer", set_command->value_type_str());
                                LOG_ERROR(status.message());
                                RecoverableError(status);
                            }
                            query_context->storage()->catalog()->ResizeProfileHistory(set_command->value_int());
                            return true;
                        }
                        case GlobalVariable::kInvalid: {
                            Status status = Status::InvalidCommand(fmt::format("unknown global variable {}", set_command->var_name()));
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                        }
                        default: {
                            Status status = Status::InvalidCommand(fmt::format("Global variable: {} is read-only", set_command->var_name()));
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                        }
                    }
                    break;
                }
                case SetScope::kConfig: {
                    Config* config = query_context->global_config();
                    GlobalOptionIndex config_index = config->GetOptionIndex(set_command->var_name());
                    switch(config_index) {
                        case GlobalOptionIndex::kLogLevel: {
                            if (set_command->value_str() == "trace") {
                                SetLogLevel(LogLevel::kTrace);
                                config->SetLogLevel(LogLevel::kTrace);
                                return true;
                            }

                            if (set_command->value_str() == "debug") {
                                SetLogLevel(LogLevel::kDebug);
                                config->SetLogLevel(LogLevel::kDebug);
                                return true;
                            }

                            if (set_command->value_str() == "info") {
                                SetLogLevel(LogLevel::kInfo);
                                config->SetLogLevel(LogLevel::kInfo);
                                return true;
                            }

                            if (set_command->value_str() == "warning") {
                                SetLogLevel(LogLevel::kWarning);
                                config->SetLogLevel(LogLevel::kWarning);
                                return true;
                            }

                            if (set_command->value_str() == "error") {
                                SetLogLevel(LogLevel::kError);
                                config->SetLogLevel(LogLevel::kError);
                                return true;
                            }

                            if (set_command->value_str() == "critical") {
                                SetLogLevel(LogLevel::kCritical);
                                config->SetLogLevel(LogLevel::kCritical);
                                return true;
                            }

                            Status status = Status::SetInvalidVarValue("log level", "trace, debug, info, warning, error, critical");
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                            break;
                        }
                        case GlobalOptionIndex::kInvalid: {
                            Status status = Status::InvalidCommand(fmt::format("Unknown config: {}", set_command->var_name()));
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                            break;
                        }
                        default: {
                            Status status = Status::InvalidCommand(fmt::format("Config {} is read-only", set_command->var_name()));
                            LOG_ERROR(status.message());
                            RecoverableError(status);
                            break;
                        }
                    }
                    break;
                }
                default: {
                    Status status = Status::InvalidCommand("Invalid set command scope, neither session nor global");
                    LOG_ERROR(status.message());
                    RecoverableError(status);
                }
            }
            break;
        }
        case CommandType::kExport: {
            ExportCmd *export_command = (ExportCmd *)(command_info_.get());
            auto profiler_record = query_context->current_session()->GetProfileRecord(export_command->file_no());
            if (profiler_record == nullptr) {
                Status status = Status::DataNotExist(fmt::format("The record does not exist: {}", export_command->file_no()));
                LOG_ERROR(status.message());
                RecoverableError(status);
            }
            LocalFileSystem fs;
            FileWriter file_writer(fs, export_command->file_name(), 128);

            auto json = QueryProfiler::Serialize(profiler_record).dump();
            file_writer.Write(json.c_str(), json.size());
            file_writer.Flush();
            break;
        }
        case CommandType::kCheckTable: {
            break;
        }
        default: {
            String error_message = fmt::format("Invalid command type: {}", command_info_->ToString());
            LOG_CRITICAL(error_message);
            UnrecoverableError(error_message);
        }
    }
    return true;
}

SizeT PhysicalCommand::TaskletCount() {
    String error_message = "Not implement: TaskletCount not Implement";
    LOG_CRITICAL(error_message);
    UnrecoverableError(error_message);
    return 0;
}

} // namespace infinity