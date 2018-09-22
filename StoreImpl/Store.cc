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
        return false;
    }

    levelDB_.reset(db);

    return true;
}


void
Store::dumpSnapshot(Core::ProtoBuf::OutputStream& stream) const
{
    //
}

/**
 * Load the tree from the given stream.
 */
void
Store::loadSnapshot(Core::ProtoBuf::InputStream& stream)
{
    //
}


Result
Store::write(const std::string& path, const std::string& contents)
{
    Result result {};
    ++numWriteAttempted;

    if (path.empty() || contents.empty()) {
        result.status = Status::INVALID_ARGUMENT;
        result.error = format("Invalid param: %s,%s", path.c_str(), contents.c_str());
        return result;
    }

    leveldb::Status status = levelDB_->Put(leveldb::WriteOptions(), path, contents);
    if (!status.ok()) {
        result.status = Status::OPERATION_ERROR;
        result.error = format("Operation failed: %s,%s", path.c_str(), contents.c_str());
        return result;
    }

    ++numWriteSuccess;
    return result;
}

Result
Store::read(const std::string& path, std::string& contents) const
{
    ++numReadAttempted;
    contents.clear();
    Result result {};

    if (path.empty()) {
        result.status = Status::INVALID_ARGUMENT;
        result.error = format("Invalid param: %s", path.c_str());
        return result;
    }

    leveldb::Status status = levelDB_->Get(leveldb::ReadOptions(), path, &contents);
    if (!status.ok()) {
        result.status = Status::OPERATION_ERROR;
        result.error = format("Operation failed: %s", path.c_str());
        return result;
    }

    ++numReadSuccess;
    return result;
}

Result
Store::remove(const std::string& path)
{
    ++numRemoveAttempted;
    Result result {};

    if (path.empty()) {
        result.status = Status::INVALID_ARGUMENT;
        result.error = format("Invalid param: %s", path.c_str());
        return result;
    }

    leveldb::Status status = levelDB_->Delete(leveldb::WriteOptions(), path);
    if (!status.ok()) {
        result.status = Status::OPERATION_ERROR;
        result.error = format("Operation failed: %s", path.c_str());
        return result;
    }

    ++numRemoveSuccess;
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
