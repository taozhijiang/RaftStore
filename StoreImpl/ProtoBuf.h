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

#include "Protocol/gen-cpp/Client.pb.h"
#include "StoreImpl/Store.h"

#ifndef LOGCABIN_STOREIMPL_PROTOBUF_H
#define LOGCABIN_STOREIMPL_PROTOBUF_H

namespace LogCabin {
namespace Store {
namespace ProtoBuf {

/**
 * Respond to a read-only request to query a Store.
 */
void
readOnlyStoreRPC(const Store& store,
                 const Protocol::Client::ReadOnlyStore::Request& request,
                 Protocol::Client::ReadOnlyStore::Response& response);

/**
 * Respond to a read-write operation on a Store.
 */
void
readWriteStoreRPC(Store& store,
                  const Protocol::Client::ReadWriteStore::Request& request,
                  Protocol::Client::ReadWriteStore::Response& response);

} // namespace LogCabin::Store::ProtoBuf
} // namespace LogCabin::Store
} // namespace LogCabin

#endif // LOGCABIN_STOREIMPL_PROTOBUF_H
