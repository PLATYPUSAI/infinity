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

export module disk_index_segment_reader;
import stl;
import memory_pool;
import segment_posting;
import index_defines;
import index_segment_reader;
import dict_reader;
import file_reader;
import posting_list_format;
import local_file_system;
import internal_types;

namespace infinity {
export class DiskIndexSegmentReader : public IndexSegmentReader {
public:
    DiskIndexSegmentReader(const String &index_dir, const String &base_name, RowID base_row_id, optionflag_t flag);
    virtual ~DiskIndexSegmentReader();

    bool GetSegmentPosting(const String &term, SegmentPosting &seg_posting, MemoryPool *session_pool) const override;

private:
    RowID base_row_id_{INVALID_ROWID};
    SharedPtr<DictionaryReader> dict_reader_;
    mutable std::mutex mutex_;
    SharedPtr<FileReader> posting_reader_;
    LocalFileSystem fs_{};
};

} // namespace infinity