/* Copyright (c) 2012 Stanford University
 * Copyright (c) 2014 Diego Ongaro
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <cassert>

#include <leveldb/comparator.h>

#include "Protocol/gen-cpp/ServerStats.pb.h"
#include "Protocol/gen-cpp/Snapshot.pb.h"
#include "Core/Debug.h"
#include "Core/StringUtil.h"
#include "StoreImpl/Store.h"

namespace LogCabin {
namespace Store {

using Core::StringUtil::format;

////////// enum Status //////////

std::ostream&
operator<<(std::ostream& os, Status status)
{
    switch (status) {
        case Status::OK:
            os << "Status::OK";
            break;
        case Status::INVALID_ARGUMENT:
            os << "Status::INVALID_ARGUMENT";
            break;
        case Status::OPERATION_ERROR:
            os << "Status::OPERATION_ERROR";
            break;
        case Status::CONDITION_NOT_MET:
            os << "Status::CONDITION_NOT_MET";
            break;
        case Status::UNKNOWN_ERROR:
            os << "Status::UNKNOWN_ERROR";
            break;
    }
    return os;
}

////////// struct Result //////////

Result::Result()
    : status(Status::OK)
    , error()
{
}


////////// class Tree //////////

Store::Store(const std::string& levelDBPath)
    : numWriteAttempted(0)
    , numWriteSuccess(0)
    , numReadAttempted(0)
    , numReadSuccess(0)
    , numRemoveAttempted(0)
    , numRemoveSuccess(0)
    , levelDBPath_(levelDBPath)
{
}

bool Store::init() {

    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, levelDBPath_, &db);

    if (!status.ok()) {
        PANIC("Open levelDB %s failed.", levelDBPath_.c_str());
        return false;
    }

    levelDB_.reset(db);

    return true;
}


void
Store::dumpSnapshot(Core::ProtoBuf::OutputStream& stream) const
{
    Snapshot::SnapshotItem total;
    Snapshot::KeyValue* ptr;

    std::unique_ptr<leveldb::Iterator> it(levelDB_->NewIterator(leveldb::ReadOptions()));
    uint64_t cnt = 0;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string key_str = key.ToString();

        leveldb::Slice value = it->value();
        std::string val_str = value.ToString();

        ptr = total.add_kv();
        ptr->set_key(key_str);
        ptr->set_value(val_str);

        ++ cnt;

    }
    stream.writeMessage(total);
    NOTICE("dumpSnapshot finished, snapshot totally store %lu items.", cnt);
}


void
Store::loadSnapshot(Core::ProtoBuf::InputStream& stream)
{

    // destroy levelDB completely first
    leveldb::DestroyDB(levelDBPath_, leveldb::Options());

    leveldb::Options create_options;
    create_options.error_if_exists = true;
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(create_options, levelDBPath_, &db);

    if (!status.ok()) {
        ERROR("Reset levelDB error: %s", levelDBPath_.c_str());
        return;
    }
    levelDB_.reset(db);

    // load data
    Snapshot::SnapshotItem total;
    stream.readMessage(total);

    leveldb::WriteOptions write_options;
    int size = total.kv_size();
    for (int i = 0; i < size; ++ i) {
        Snapshot::KeyValue kv = total.kv(i);
        leveldb::Status status = levelDB_->Put(write_options, kv.key(), kv.value());
        if (!status.ok()) {
            ERROR("Restoring %s:%s failed, give up...",
                            kv.key().c_str(), kv.value().c_str());
            return;
        }
    }
}


Result
Store::checkCondition(const std::string& key,
                      const std::string& content) const
{
    Result result;

    if (key == "[[keepalive-reserved-path]]") {
        result.status = Status::CONDITION_NOT_MET;
        result.error = format("keepalive reserved path: %s", key.c_str());
        return result;
    }

    return result;
}



Result
Store::write(const std::string& key, const std::string& content)
{
    Result result {};
    ++numWriteAttempted;

    if (key.empty() || content.empty()) {
        result.status = Status::INVALID_ARGUMENT;
        result.error = format("Invalid param: %s,%s", key.c_str(), content.c_str());
        return result;
    }


    leveldb::WriteOptions options;
    options.sync = true;
    leveldb::Status status = levelDB_->Put(options, key, content);
    if (!status.ok()) {
        result.status = Status::OPERATION_ERROR;
        result.error = format("Operation failed: %s,%s", key.c_str(), content.c_str());
        return result;
    }

    ++numWriteSuccess;
    return result;
}

Result
Store::read(const std::string& key, std::string& content) const
{
    ++numReadAttempted;
    content.clear();
    Result result {};

    if (key.empty()) {
        result.status = Status::INVALID_ARGUMENT;
        result.error = format("Invalid param: %s", key.c_str());
        return result;
    }

    leveldb::Status status = levelDB_->Get(leveldb::ReadOptions(), key, &content);
    if (!status.ok()) {
        result.status = Status::OPERATION_ERROR;
        result.error = format("Operation failed: %s", key.c_str());
        return result;
    }

    ++numReadSuccess;
    return result;
}

Result
Store::remove(const std::string& key)
{
    ++numRemoveAttempted;
    Result result {};

    if (key.empty()) {
        result.status = Status::INVALID_ARGUMENT;
        result.error = format("Invalid param: %s", key.c_str());
        return result;
    }

    leveldb::WriteOptions options;
    options.sync = true;
    leveldb::Status status = levelDB_->Delete(options, key);
    if (!status.ok()) {
        result.status = Status::OPERATION_ERROR;
        result.error = format("Operation failed: %s", key.c_str());
        return result;
    }

    ++numRemoveSuccess;
    return result;
}



Result
Store::range(const std::string& start, const std::string& end, uint64_t limit,
             std::vector<std::string>& range_store) const {

    ++numReadAttempted;
    Result result {};

    std::unique_ptr<leveldb::Iterator> it(levelDB_->NewIterator(leveldb::ReadOptions()));
    uint64_t cnt = 0;
    leveldb::Options options;

    if (start.empty()) {
        it->SeekToFirst();
    } else {
        it->Seek(start);
    }

    for ( /* */; it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string key_str = key.ToString();

        // leveldb::Slice value = it->value();
        // std::string val_str = value.ToString();

        if (limit && ++cnt > limit) {
            break;
        }

        if (!end.empty() && options.comparator->Compare(key, end) > 0) {
            break;
        }

        range_store.push_back(key_str);
    }

    ++numReadSuccess;
    return result;
}

Result
Store::search(const std::string& search_key, uint64_t limit,
              std::vector<std::string>& search_store) const {

    ++numReadAttempted;
    Result result {};

    std::unique_ptr<leveldb::Iterator> it(levelDB_->NewIterator(leveldb::ReadOptions()));
    uint64_t cnt = 0;
    leveldb::Options options;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string key_str = key.ToString();

        // leveldb::Slice value = it->value();
        // std::string val_str = value.ToString();

        if (key_str.find(search_key) != std::string::npos) {
            search_store.push_back(key_str);
        }

        if (limit && ++cnt > limit) {
            break;
        }
    }

    ++numReadSuccess;
    return result;
}

Result
Store::stat(const std::string& client, std::string& content) const {

    Result result {};

    content = "Stat response TODO";
    return result;
}


void
Store::updateServerStats(Protocol::ServerStats::Store& tstats) const
{
    tstats.set_num_write_attempted(
        numWriteAttempted);
    tstats.set_num_write_success(
        numWriteSuccess);
    tstats.set_num_read_attempted(
        numReadAttempted);
    tstats.set_num_read_success(
        numReadSuccess);
    tstats.set_num_remove_attempted(
        numRemoveAttempted);
    tstats.set_num_remove_success(
        numRemoveSuccess);
}

} // namespace LogCabin::Store
} // namespace LogCabin
