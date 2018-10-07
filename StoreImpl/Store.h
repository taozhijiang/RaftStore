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

#include <map>
#include <string>
#include <vector>
#include <memory>

#include <boost/noncopyable.hpp>

#include <leveldb/db.h>

#include "Core/ProtoBuf.h"

#ifndef LOGCABIN_STOREIMPL_STORE_H
#define LOGCABIN_STOREIMPL_STORE_H

namespace LogCabin {

// forward declaration
namespace Protocol {
class ServerStats_Store;
}

namespace Store {

/**
 * Status codes returned by Tree operations.
 */
enum class Status {

    /**
     * The operation completed successfully.
     */
    OK = 0,

    /**
     * If an argument is malformed (for example, a path that does not start
     * with a slash).
     */
    INVALID_ARGUMENT = 1,

    /**
     * May low level storage return error ( for example, read path
     * now exists, etc).
     */
    OPERATION_ERROR = 2,

    /**
     * Precondition check, may read/write reserved path
     * [[keepalive-reserved-path]]
     *
     */
    CONDITION_NOT_MET = 3,

    /**
     * If a directory exists where a file is required or a file exists where
     * a directory is required.
     */
    UNKNOWN_ERROR = 4,

};

/**
 * Print a status code to a stream.
 */
std::ostream&
operator<<(std::ostream& os, Status status);

/**
 * Returned by Tree operations; contain a status code and an error message.
 */
struct Result {
    /**
     * Default constructor. Sets status to OK and error to the empty string.
     */
    Result();
    /**
     * A code for whether an operation succeeded or why it did not. This is
     * meant to be used programmatically.
     */
    Status status;
    /**
     * If status is not OK, this is a human-readable message describing what
     * went wrong.
     */
    std::string error;
};

/**
 * This is an in-memory, hierarchical key-value store.
 * TODO(ongaro): Document how this fits into the rest of the system.
 */
class Store: public boost::noncopyable {
  public:
    /**
     * Constructor.
     */
    explicit Store(const std::string& levelDBPath);
    bool init();

    /**
     * Write the levelDB store to the given stream.
     */
    void dumpSnapshot(Core::ProtoBuf::OutputStream& stream) const;

    /**
     * Load the total levelDB from the given stream.
     * \warning
     *      Original levelDB store will remove completedly first.
     */
    void loadSnapshot(Core::ProtoBuf::InputStream& stream);

    /**
     * Verify that the value at key has the given contents.
     * Currently only used for filter away KeepAlive RPC.
     * \param key
     *      The path to the file that must have the contents specified in
     *      'content'.
     * \param content
     *      The contents that the file specified by 'key' should
     *      have for an OK response. An OK response is also returned
     *      if 'content' is the empty string and the file specified
     *      by 'key' does not exist.
     * \return
     *      Status and error message. Possible errors are:
     *       - CONDITION_NOT_MET upon any error.
     */
    Result
    checkCondition(const std::string& key,
                   const std::string& content = "") const;

    /**
     * Set the value of a key.
     * \param key
     *      The key where there should be a file with the given
     *      content after this call.
     * \param content
     *      The new value associated with the key.
     * \return
     *      Status and error message. Possible errors are:
     *       - INVALID_ARGUMENT if key is malformed.
     *       - OPERATION_ERROR if levelDB operation return fail.
     */
    Result
    write(const std::string& key, const std::string& content);

    /**
     * Get the value of a key.
     * \param key
     *      The key of the file whose content to read.
     * \param content
     *      The current value associated with the key.
     * \return
     *      Status and error message. Possible errors are:
     *       - INVALID_ARGUMENT if key is malformed.
     *       - OPERATION_ERROR if levelDB operation return fail.
     */
    Result
    read(const std::string& key, std::string& content) const;

    /**
     * Make sure a file does not exist.
     * \param key
     *      The key where there should not be a file after this
     *      call.
     * \return
     *      Status and error message. Possible errors are:
     *       - INVALID_ARGUMENT if path is malformed.
     *       - OPERATION_ERROR if levelDB operation return fail.
     */
    Result
    remove(const std::string& key);

    Result
    range(const std::string& start, const std::string& end, uint64_t limit,
          std::vector<std::string>& range_store) const;

    Result
    search(const std::string& search_key, uint64_t limit,
           std::vector<std::string>& search_store) const;

    Result
    stat(const std::string& client, std::string& content) const;

    /**
     * Add metrics about the levelDB operation to the given
     * structure.
     */
    void
    updateServerStats(Protocol::ServerStats_Store& tstats) const;

  private:

    // Server stats collected in updateServerStats.
    // Note that when a condition fails, the operation is not invoked,
    // so operations whose conditions fail are not counted as 'Attempted'.
    uint64_t numWriteAttempted;
    uint64_t numWriteSuccess;
    mutable uint64_t numReadAttempted;
    mutable uint64_t numReadSuccess;
    uint64_t numRemoveAttempted;
    uint64_t numRemoveDone;
    uint64_t numRemoveSuccess;

    std::string levelDBPath_;
    std::unique_ptr<leveldb::DB> levelDB_;
};


} // namespace LogCabin::Store
} // namespace LogCabin

#endif // LOGCABIN_STOREIMPL_STORE_H
