/* Copyright (c) 2012 Stanford University
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

#include "Core/Debug.h"
#include "Core/ProtoBuf.h"
#include "StoreImpl/ProtoBuf.h"

namespace LogCabin {
namespace Store {
namespace ProtoBuf {

namespace PC = LogCabin::Protocol::Client;

void
readOnlyStoreRPC(const Store& store,
                 const PC::ReadOnlyStore::Request& request,
                 PC::ReadOnlyStore::Response& response)
{
    Result result;

    result = store.checkCondition(request.read().path());
    if (result.status != Status::OK) {

    } else if (request.has_stat()) {

        std::string content;
        result = store.stat(request.stat().client(), content);
        response.mutable_stat()->set_content(content);

    } else if (request.has_read()) {

        std::string content;
        result = store.read(request.read().path(), content);
        response.mutable_read()->set_content(content);

    } else if (request.has_range()) {

        std::vector<std::string> range_store;
        std::string start_key = request.range().start_key();
        std::string end_key = request.range().end_key();
        uint64_t limit = request.range().limit();
        result = store.range(start_key, end_key, limit, range_store);

        *response.mutable_range()->mutable_contents()
                = {range_store.begin(), range_store.end()};

    } else if (request.has_search()) {

        std::vector<std::string> search_store;
        std::string search_key = request.search().search_key();
        uint64_t limit = request.search().limit();
        result = store.search(search_key, limit, search_store);

        *response.mutable_search()->mutable_contents()
                = {search_store.begin(), search_store.end()};


    } else {
        PANIC("Unexpected request: %s",
              Core::ProtoBuf::dumpString(request).c_str());
    }
    response.set_status(static_cast<PC::Status>(result.status));
    if (result.status != Status::OK)
        response.set_error(result.error);
}

void
readWriteStoreRPC(Store& store,
                  const PC::ReadWriteStore::Request& request,
                  PC::ReadWriteStore::Response& response)
{
    Result result;

    if (request.has_write()) {

        result = store.checkCondition(request.write().path());
        if (result.status != Status::OK)
            goto ret;
        result = store.write(request.write().path(),
                             request.write().content());

    } else if (request.has_remove()) {

        result = store.checkCondition(request.remove().path());
        if (result.status != Status::OK)
            goto ret;
        result = store.remove(request.remove().path());

    } else {
        PANIC("Unexpected request: %s",
              Core::ProtoBuf::dumpString(request).c_str());
    }

ret:
    response.set_status(static_cast<PC::Status>(result.status));
    if (result.status != Status::OK)
        response.set_error(result.error);
}

} // namespace LogCabin::Tree::ProtoBuf
} // namespace LogCabin::Tree
} // namespace LogCabin
