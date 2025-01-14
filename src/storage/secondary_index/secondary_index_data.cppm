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

export module secondary_index_data;

import stl;
import default_values;
import file_system;
import file_system_type;
import infinity_exception;
import column_vector;
import third_party;
import secondary_index_pgm;
import logical_type;
import internal_types;
import data_type;
import segment_entry;
import buffer_handle;
import logger;

namespace infinity {
class ChunkIndexEntry;

template <typename T>
concept KeepOrderedSelf = IsAnyOf<T, TinyIntT, SmallIntT, IntegerT, BigIntT, FloatT, DoubleT>;

template <typename T>
concept ConvertToOrderedI32 = IsAnyOf<T, DateT, TimeT>;

template <typename T>
concept ConvertToOrderedI64 = IsAnyOf<T, DateTimeT, TimestampT>;

template <typename ValueT>
struct ConvertToOrdered {
    static_assert(false, "type not supported");
};

template <KeepOrderedSelf T>
struct ConvertToOrdered<T> {
    using type = T;
};

template <ConvertToOrderedI32 T>
struct ConvertToOrdered<T> {
    using type = i32;
};

template <ConvertToOrderedI64 T>
struct ConvertToOrdered<T> {
    using type = i64;
};

export template <typename T>
    requires KeepOrderedSelf<T> or ConvertToOrderedI32<T> or ConvertToOrderedI64<T>
using ConvertToOrderedType = ConvertToOrdered<T>::type;

export template <typename RawValueType>
ConvertToOrderedType<RawValueType> ConvertToOrderedKeyValue(RawValueType value) {
    static_assert(false, "type not supported");
}

export template <KeepOrderedSelf RawValueType>
ConvertToOrderedType<RawValueType> ConvertToOrderedKeyValue(RawValueType value) {
    return value;
}

// DateT, TimeT
export template <ConvertToOrderedI32 RawValueType>
ConvertToOrderedType<RawValueType> ConvertToOrderedKeyValue(RawValueType value) {
    return value.GetValue();
}

// DateTimeT, TimestampT
export template <ConvertToOrderedI64 RawValueType>
ConvertToOrderedType<RawValueType> ConvertToOrderedKeyValue(RawValueType value) {
    return value.GetEpochTime();
}

template <typename T>
LogicalType GetLogicalType = LogicalType::kInvalid;

template <>
LogicalType GetLogicalType<FloatT> = LogicalType::kFloat;

template <>
LogicalType GetLogicalType<DoubleT> = LogicalType::kDouble;

template <>
LogicalType GetLogicalType<TinyIntT> = LogicalType::kTinyInt;

template <>
LogicalType GetLogicalType<SmallIntT> = LogicalType::kSmallInt;

template <>
LogicalType GetLogicalType<IntegerT> = LogicalType::kInteger;

template <>
LogicalType GetLogicalType<BigIntT> = LogicalType::kBigInt;

export class SecondaryIndexData {
protected:
    u32 chunk_row_count_ = 0;
    // pgm index
    // will always be loaded
    UniquePtr<SecondaryPGMIndex> pgm_index_;

public:
    explicit SecondaryIndexData(u32 chunk_row_count) : chunk_row_count_(chunk_row_count) {}

    virtual ~SecondaryIndexData() = default;

    [[nodiscard]] inline auto SearchPGM(const void *val_ptr) const {
        if (!pgm_index_) {
            String error_message = "Not initialized yet.";
            LOG_CRITICAL(error_message);
            UnrecoverableError(error_message);
        }
        return pgm_index_->SearchIndex(val_ptr);
    }

    [[nodiscard]] inline u32 GetChunkRowCount() const { return chunk_row_count_; }

    virtual void SaveIndexInner(FileHandler &file_handler) const = 0;

    virtual void ReadIndexInner(FileHandler &file_handler) = 0;

    virtual void InsertData(void *ptr, SharedPtr<ChunkIndexEntry> &chunk_index) = 0;

    virtual void InsertMergeData(Vector<ChunkIndexEntry *> &old_chunks, SharedPtr<ChunkIndexEntry> &merged_chunk_index_entry) = 0;
};

export SecondaryIndexData *GetSecondaryIndexData(const SharedPtr<DataType> &data_type, u32 chunk_row_count, bool allocate);

export u32 GetSecondaryIndexDataPairSize(const SharedPtr<DataType> &data_type);

} // namespace infinity