/* Copyright (c) 2012 Stanford University
 * Copyright (c) 2014-2015 Diego Ongaro
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

#include <string.h>

#include "include/LogCabin/Client.h"
#include "Client/ClientImpl.h"
#include "Core/StringUtil.h"

namespace LogCabin {
namespace Client {

////////// Server //////////

Server::Server(uint64_t serverId, const std::string& addresses)
    : serverId(serverId)
    , addresses(addresses)
{
}

Server::Server()
    : serverId(~0UL)
    , addresses("")
{
}

Server::Server(const Server& other)
    : serverId(other.serverId)
    , addresses(other.addresses)
{
}

Server::~Server()
{
}

Server&
Server::operator=(const Server& other)
{
    serverId = other.serverId;
    addresses = other.addresses;
    return *this;
}

////////// ConfigurationResult //////////

ConfigurationResult::ConfigurationResult()
    : status(OK)
    , badServers()
    , error()
{
}

ConfigurationResult::~ConfigurationResult()
{
}

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
        case Status::LOOKUP_ERROR:
            os << "Status::LOOKUP_ERROR";
            break;
        case Status::TYPE_ERROR:
            os << "Status::TYPE_ERROR";
            break;
        case Status::TIMEOUT:
            os << "Status::TIMEOUT";
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

////////// TreeDetails //////////

/**
 * Implementation-specific members of Client::Tree.
 */
class StoreDetails {
  public:
    StoreDetails(std::shared_ptr<ClientImpl> clientImpl)
        : clientImpl(clientImpl)
        , timeoutNanos(0)
    {
    }
    /**
     * Client implementation.
     */
    std::shared_ptr<ClientImpl> clientImpl;
    /**
     * If nonzero, a relative timeout in nanoseconds for all Store operations.
     */
    uint64_t timeoutNanos;
};


////////// Store //////////

Store::Store(std::shared_ptr<ClientImpl> clientImpl)
    : mutex()
    , storeDetails(new StoreDetails(clientImpl))
{
}

Store::Store(const Store& other)
    : mutex()
    , storeDetails(other.getStoreDetails())
{
}

Store&
Store::operator=(const Store& other)
{
    // Hold one lock at a time to avoid deadlock and handle self-assignment.
    std::shared_ptr<const StoreDetails> otherStoreDetails =
                                            other.getStoreDetails();
    std::lock_guard<std::mutex> lockGuard(mutex);
    storeDetails = otherStoreDetails;
    return *this;
}

uint64_t
Store::getTimeout() const
{
    std::shared_ptr<const StoreDetails> storeDetails = getStoreDetails();
    return storeDetails->timeoutNanos;
}

void
Store::setTimeout(uint64_t nanoseconds)
{
    std::lock_guard<std::mutex> lockGuard(mutex);
    std::shared_ptr<StoreDetails> newStoreDetails(new StoreDetails(*storeDetails));
    newStoreDetails->timeoutNanos = nanoseconds;
    storeDetails = newStoreDetails;
}

Result
Store::write(const std::string& path, const std::string& contents)
{
    std::shared_ptr<const StoreDetails> storeDetails = getStoreDetails();
    return storeDetails->clientImpl->write(
        path,
        contents,
        ClientImpl::absTimeout(storeDetails->timeoutNanos));
}

Result
Store::read(const std::string& path, std::string& contents) const
{
    std::shared_ptr<const StoreDetails> storeDetails = getStoreDetails();
    return storeDetails->clientImpl->read(
        path,
        ClientImpl::absTimeout(storeDetails->timeoutNanos),
        contents);
}

Result
Store::remove(const std::string& path)
{
    std::shared_ptr<const StoreDetails> storeDetails = getStoreDetails();
    return storeDetails->clientImpl->remove(
        path,
        ClientImpl::absTimeout(storeDetails->timeoutNanos));
}

std::shared_ptr<const StoreDetails>
Store::getStoreDetails() const
{
    std::shared_ptr<const StoreDetails> ret;
    std::lock_guard<std::mutex> lockGuard(mutex);
    ret = storeDetails;
    return ret;
}


////////// Cluster //////////

Cluster::Cluster(const std::string& hosts,
                 const std::map<std::string, std::string>& options)
    : clientImpl(std::make_shared<ClientImpl>(options))
{
#if DEBUG // for testing purposes only
    if (hosts == "-MOCK-SKIP-INIT-")
        return;
#endif
    clientImpl->init(hosts);
}

Cluster::~Cluster()
{
}

std::pair<uint64_t, Configuration>
Cluster::getConfiguration() const
{
    return clientImpl->getConfiguration();
}

ConfigurationResult
Cluster::setConfiguration(uint64_t oldId,
                          const Configuration& newConfiguration)
{
    return clientImpl->setConfiguration(oldId, newConfiguration);
}


Result
Cluster::getServerInfo(const std::string& host,
                       uint64_t timeoutNanoseconds,
                       Server& info)
{
    return clientImpl->getServerInfo(
                host,
                ClientImpl::absTimeout(timeoutNanoseconds),
                info);
}

Result
Cluster::getServerStats(const std::string& host,
                        uint64_t timeoutNanoseconds,
                        Protocol::ServerStats& stats)
{

    Protocol::ServerControl::ServerStatsGet::Request request;
    Protocol::ServerControl::ServerStatsGet::Response response;
    Result result = clientImpl->serverControl(
                host,
                ClientImpl::absTimeout(timeoutNanoseconds),
                Protocol::ServerControl::OpCode::SERVER_STATS_GET,
                request,
                response);
    stats = response.server_stats();
    return result;
}

Store
Cluster::getStore()
{
    return Store(clientImpl);
}

} // namespace LogCabin::Client
} // namespace LogCabin
