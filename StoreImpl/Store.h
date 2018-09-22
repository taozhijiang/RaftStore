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
     * If a file or directory that is required for the operation does not
     * exist.
     */
    OPERATION_ERROR = 2,

    /**
     * If a directory exists where a file is required or a file exists where
     * a directory is required.
     */
    UNKNOWN_ERROR = 3,

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
     * Write the tree to the given stream.
     */
    void dumpSnapshot(Core::ProtoBuf::OutputStream& stream) const;

    /**
     * Load the tree from the given stream.
     * \warning
     *      This will blow away any existing files and directories.
     */
    void loadSnapshot(Core::ProtoBuf::InputStream& stream);

    /**
     * Set the value of a file.
     * \param path
     *      The path where there should be a file with the given contents after
     *      this call.
     * \param contents
     *      The new value associated with the file.
     * \return
     *      Status and error message. Possible errors are:
     *       - INVALID_ARGUMENT if path is malformed.
     *       - INVALID_ARGUMENT if contents are too large to fit in a file.
     *       - LOOKUP_ERROR if a parent of path does not exist.
     *       - TYPE_ERROR if a parent of path is a file.
     *       - TYPE_ERROR if path exists but is a directory.
     */
    Result
    write(const std::string& path, const std::string& contents);

    /**
     * Get the value of a file.
     * \param path
     *      The path of the file whose contents to read.
     * \param contents
     *      The current value associated with the file.
     * \return
     *      Status and error message. Possible errors are:
     *       - INVALID_ARGUMENT if path is malformed.
     *       - LOOKUP_ERROR if a parent of path does not exist.
     *       - LOOKUP_ERROR if path does not exist.
     *       - TYPE_ERROR if a parent of path is a file.
     *       - TYPE_ERROR if path is a directory.
     */
    Result
    read(const std::string& path, std::string& contents) const;

    /**
     * Make sure a file does not exist.
     * \param path
     *      The path where there should not be a file after this call.
     * \return
     *      Status and error message. Possible errors are:
     *       - INVALID_ARGUMENT if path is malformed.
     *       - TYPE_ERROR if a parent of path is a file.
     *       - TYPE_ERROR if path exists but is a directory.
     */
    Result
    remove(const std::string& path);

    /**
     * Add metrics about the tree to the given structure.
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
