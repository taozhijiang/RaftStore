/* Copyright (c) 2012-2014 Stanford University
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

#include <algorithm>

#include "Core/Debug.h"
#include "Client/ClientImpl.h"
#include "Core/ProtoBuf.h"
#include "Core/StringUtil.h"
#include "Protocol/Common.h"
#include "RPC/Address.h"
#include "RPC/ClientRPC.h"
#include "RPC/ClientSession.h"

namespace LogCabin {
namespace Client {

namespace {
/**
 * Parse an error response out of a ProtoBuf and into a Result object.
 */
template<typename Message>
Result
storeError(const Message& response)
{
    Result result;
    result.error = response.error();
    switch (response.status()) {
        case Protocol::Client::Status::OK:
            result.status = Status::OK;
            break;
        case Protocol::Client::Status::INVALID_ARGUMENT:
            result.status = Status::INVALID_ARGUMENT;
            break;
        case Protocol::Client::Status::LOOKUP_ERROR:
            result.status = Status::LOOKUP_ERROR;
            break;
        case Protocol::Client::Status::TYPE_ERROR:
            result.status = Status::TYPE_ERROR;
            break;
        case Protocol::Client::Status::TIMEOUT:
            result.status = Status::TIMEOUT;
            break;
        case Protocol::Client::Status::SESSION_EXPIRED:
            PANIC("The client's session to the cluster expired. This is a "
                  "fatal error, since without a session the servers can't "
                  "tell if retried requests were already applied or not.");
            break;
        default:
            result.status = Status::INVALID_ARGUMENT;
            result.error = Core::StringUtil::format(
                "Did not understand status code in response (%u). "
                "Original error was: %s",
                response.status(),
                response.error().c_str());
            break;
    }
    return result;
}

/**
 * Wrapper around LeaderRPC::call() that repackages a timeout as a
 * ReadOnlyTree status and error message.
 */
void
storeCall(LeaderRPCBase& leaderRPC,
         const Protocol::Client::ReadOnlyStore::Request& request,
         Protocol::Client::ReadOnlyStore::Response& response,
         ClientImpl::TimePoint timeout)
{
    VERBOSE("Calling read-only tree query with request:\n%s",
            Core::StringUtil::trim(
                Core::ProtoBuf::dumpString(request)).c_str());
    LeaderRPC::Status status;
    Protocol::Client::StateMachineQuery::Request qrequest;
    Protocol::Client::StateMachineQuery::Response qresponse;
    *qrequest.mutable_store() = request;
    status = leaderRPC.call(Protocol::Client::OpCode::STATE_MACHINE_QUERY,
                            qrequest, qresponse, timeout);
    switch (status) {
        case LeaderRPC::Status::OK:
            response = *qresponse.mutable_store();
            VERBOSE("Reply to read-only tree query:\n%s",
                    Core::StringUtil::trim(
                        Core::ProtoBuf::dumpString(response)).c_str());
            break;
        case LeaderRPC::Status::TIMEOUT:
            response.set_status(Protocol::Client::Status::TIMEOUT);
            response.set_error("Client-specified timeout elapsed");
            VERBOSE("Timeout elapsed on read-only tree query");
            break;
        case LeaderRPC::Status::INVALID_REQUEST:
            // TODO(ongaro): Once any new Tree request types are introduced,
            // this PANIC will need to move up the call stack, so that we can
            // try a new-style request and then ask for forgiveness if it
            // fails. Same for the read-write tree calls below.
            PANIC("The server and/or replicated state machine doesn't support "
                  "the read-only tree query or claims the request is "
                  "malformed. Request is: %s",
                  Core::ProtoBuf::dumpString(request).c_str());
    }
}

/**
 * Wrapper around LeaderRPC::call() that repackages a timeout as a
 * ReadWriteTree status and error message. Also checks whether getRPCInfo
 * timed out.
 */
void
storeCall(LeaderRPCBase& leaderRPC,
         const Protocol::Client::ReadWriteStore::Request& request,
         Protocol::Client::ReadWriteStore::Response& response,
         ClientImpl::TimePoint timeout)
{
    VERBOSE("Calling read-write tree command with request:\n%s",
            Core::StringUtil::trim(
                Core::ProtoBuf::dumpString(request)).c_str());
    Protocol::Client::StateMachineCommand::Request crequest;
    Protocol::Client::StateMachineCommand::Response cresponse;
    *crequest.mutable_store() = request;
    LeaderRPC::Status status;
    if (request.exactly_once().client_id() == 0) {
        VERBOSE("Already timed out on establishing session for read-write "
                "tree command");
        status = LeaderRPC::Status::TIMEOUT;
    } else {
        status = leaderRPC.call(Protocol::Client::OpCode::STATE_MACHINE_COMMAND,
                                crequest, cresponse, timeout);
    }

    switch (status) {
        case LeaderRPC::Status::OK:
            response = *cresponse.mutable_store();
            VERBOSE("Reply to read-write tree command:\n%s",
                    Core::StringUtil::trim(
                        Core::ProtoBuf::dumpString(response)).c_str());
            break;
        case LeaderRPC::Status::TIMEOUT:
            response.set_status(Protocol::Client::Status::TIMEOUT);
            response.set_error("Client-specified timeout elapsed");
            VERBOSE("Timeout elapsed on read-write tree command");
            break;
        case LeaderRPC::Status::INVALID_REQUEST:
            PANIC("The server and/or replicated state machine doesn't support "
                  "the read-write tree command or claims the request is "
                  "malformed. Request is: %s",
                  Core::ProtoBuf::dumpString(request).c_str());
    }
}


} // anonymous namespace

using Protocol::Client::OpCode;


////////// class ClientImpl::ExactlyOnceRPCHelper //////////

ClientImpl::ExactlyOnceRPCHelper::ExactlyOnceRPCHelper(ClientImpl* client)
    : client(client)
    , mutex()
    , outstandingRPCNumbers()
    , clientId(0)
    , nextRPCNumber(1)
    , keepAliveCV()
    , exiting(false)
    , lastKeepAliveStart(TimePoint::min())
      // TODO(ongaro): set dynamically based on cluster configuration
    , keepAliveInterval(std::chrono::milliseconds(60 * 1000))
    , sessionCloseTimeout(std::chrono::milliseconds(
        client->config.read<uint64_t>(
            "sessionCloseTimeoutMilliseconds",
            client->config.read<uint64_t>(
                "tcpConnectTimeoutMilliseconds",
                1000))))
    , keepAliveCall()
    , keepAliveThread()
{
}

ClientImpl::ExactlyOnceRPCHelper::~ExactlyOnceRPCHelper()
{
}

void
ClientImpl::ExactlyOnceRPCHelper::exit()
{
    {
        std::lock_guard<Core::Mutex> lockGuard(mutex);
        exiting = true;
        keepAliveCV.notify_all();
        if (keepAliveCall)
            keepAliveCall->cancel();
        if (clientId > 0) {
            Protocol::Client::StateMachineCommand::Request request;
            Protocol::Client::StateMachineCommand::Response response;
            request.mutable_close_session()->set_client_id(clientId);
            LeaderRPC::Status status = client->leaderRPC->call(
                    OpCode::STATE_MACHINE_COMMAND,
                    request,
                    response,
                    ClientImpl::Clock::now() + sessionCloseTimeout);
            switch (status) {
                case LeaderRPC::Status::OK:
                    break;
                case LeaderRPC::Status::TIMEOUT:
                    using Core::StringUtil::toString;
                    WARNING("Could not definitively close client session %lu "
                            "within timeout (%s). It may remain open until it "
                            "expires.",
                            clientId,
                            toString(sessionCloseTimeout).c_str());
                    break;
                case LeaderRPC::Status::INVALID_REQUEST:
                    WARNING("The server and/or replicated state machine "
                            "doesn't support the CloseSession command or "
                            "claims the request is malformed. This client's "
                            "session (%lu) will remain open until it expires. "
                            "Consider upgrading your servers (this command "
                            "was introduced in state machine version 2).",
                            clientId);
                    break;
            }
        }
    }
    if (keepAliveThread.joinable())
        keepAliveThread.join();
}

Protocol::Client::ExactlyOnceRPCInfo
ClientImpl::ExactlyOnceRPCHelper::getRPCInfo(TimePoint timeout)
{
    std::lock_guard<Core::Mutex> lockGuard(mutex);
    return getRPCInfo(Core::HoldingMutex(lockGuard), timeout);
}

void
ClientImpl::ExactlyOnceRPCHelper::doneWithRPC(
        const Protocol::Client::ExactlyOnceRPCInfo& rpcInfo)
{
    std::lock_guard<Core::Mutex> lockGuard(mutex);
    doneWithRPC(rpcInfo, Core::HoldingMutex(lockGuard));
}

Protocol::Client::ExactlyOnceRPCInfo
ClientImpl::ExactlyOnceRPCHelper::getRPCInfo(
        Core::HoldingMutex holdingMutex,
        TimePoint timeout)
{
    Protocol::Client::ExactlyOnceRPCInfo rpcInfo;
    if (client == NULL) {
        // Filling in rpcInfo is disabled for some unit tests, since it's
        // easier if they treat rpcInfo opaquely.
        return rpcInfo;
    }
    if (clientId == 0) {
        lastKeepAliveStart = Clock::now();
        Protocol::Client::StateMachineCommand::Request request;
        Protocol::Client::StateMachineCommand::Response response;
        request.mutable_open_session();
        LeaderRPC::Status status =
            client->leaderRPC->call(OpCode::STATE_MACHINE_COMMAND,
                                    request,
                                    response,
                                    timeout);
        switch (status) {
            case LeaderRPC::Status::OK:
                break;
            case LeaderRPC::Status::TIMEOUT:
                rpcInfo.set_client_id(0);
                return rpcInfo;
            case LeaderRPC::Status::INVALID_REQUEST:
                PANIC("The server and/or replicated state machine doesn't "
                      "support the OpenSession command or claims the request "
                      "is malformed");
        }
        clientId = response.open_session().client_id();
        assert(clientId > 0);
        keepAliveThread = std::thread(
            &ClientImpl::ExactlyOnceRPCHelper::keepAliveThreadMain,
            this);
    }

    lastKeepAliveStart = Clock::now();
    keepAliveCV.notify_all();
    rpcInfo.set_client_id(clientId);
    uint64_t rpcNumber = nextRPCNumber;
    ++nextRPCNumber;
    rpcInfo.set_rpc_number(rpcNumber);
    outstandingRPCNumbers.insert(rpcNumber);
    rpcInfo.set_first_outstanding_rpc(*outstandingRPCNumbers.begin());
    return rpcInfo;
}

void
ClientImpl::ExactlyOnceRPCHelper::doneWithRPC(
                    const Protocol::Client::ExactlyOnceRPCInfo& rpcInfo,
                    Core::HoldingMutex holdingMutex)
{
    outstandingRPCNumbers.erase(rpcInfo.rpc_number());
}

void
ClientImpl::ExactlyOnceRPCHelper::keepAliveThreadMain()
{
    std::unique_lock<Core::Mutex> lockGuard(mutex);
    while (!exiting) {
        TimePoint nextKeepAlive;
        if (keepAliveInterval.count() > 0) {
            nextKeepAlive = lastKeepAliveStart + keepAliveInterval;
        } else {
            nextKeepAlive = TimePoint::max();
        }
        if (Clock::now() > nextKeepAlive) {
            Protocol::Client::StateMachineCommand::Request request;
            Protocol::Client::ReadWriteStore::Request& trequest =
                *request.mutable_store();
            *trequest.mutable_exactly_once() = getRPCInfo(
                Core::HoldingMutex(lockGuard),
                TimePoint::max());
            trequest.mutable_write()->set_path("[[keepalive-reserved-path]]");
            trequest.mutable_write()->set_content("you shouldn't see this!");
            Protocol::Client::StateMachineCommand::Response response;
            keepAliveCall = client->leaderRPC->makeCall();
            keepAliveCall->start(OpCode::STATE_MACHINE_COMMAND, request,
                                 TimePoint::max());
            LeaderRPCBase::Call::Status callStatus;
            {
                // release lock to allow concurrent cancellation
                Core::MutexUnlock<Core::Mutex> unlockGuard(lockGuard);
                callStatus = keepAliveCall->wait(response, TimePoint::max());
            }
            keepAliveCall.reset();
            switch (callStatus) {
                case LeaderRPCBase::Call::Status::OK:
                    break;
                case LeaderRPCBase::Call::Status::RETRY:
                    continue; // retry outer loop
                case LeaderRPCBase::Call::Status::TIMEOUT:
                    PANIC("Unexpected timeout for keep-alive");
                case LeaderRPCBase::Call::Status::INVALID_REQUEST:
                    PANIC("The server rejected our keep-alive request (Tree "
                          "write with unmet condition) as invalid");
            }
            doneWithRPC(trequest.exactly_once(),
                        Core::HoldingMutex(lockGuard));
            const Protocol::Client::ReadWriteStore::Response& tresponse =
                response.store();
            if (tresponse.status() !=
                Protocol::Client::Status::CONDITION_NOT_MET) {
                WARNING("Keep-alive write should have failed its condition. "
                        "Unexpected status was %d: %s",
                        tresponse.status(),
                        tresponse.error().c_str());
            }
            continue;
        }
        keepAliveCV.wait_until(lockGuard, nextKeepAlive);
    }
}

////////// class ClientImpl //////////

ClientImpl::TimePoint
ClientImpl::absTimeout(uint64_t relTimeoutNanos)
{
    if (relTimeoutNanos == 0)
        return ClientImpl::TimePoint::max();
    ClientImpl::TimePoint now = ClientImpl::Clock::now();
    ClientImpl::TimePoint then =
        now + std::chrono::nanoseconds(relTimeoutNanos);
    if (then < now) // overflow
        return ClientImpl::TimePoint::max();
    else
        return then;
}

ClientImpl::ClientImpl(const std::map<std::string, std::string>& options)
    : config(options)
    , eventLoop()
    , clusterUUID()
    , sessionManager(eventLoop, config)
    , sessionCreationBackoff(5,                   // 5 new connections per
                             100UL * 1000 * 1000) // 100 ms
    , hosts()
    , leaderRPC()             // set in init()
    , exactlyOnceRPCHelper(this)
    , eventLoopThread()
{
    NOTICE("Configuration settings:\n"
           "# begin config\n"
           "%s"
           "# end config",
           Core::StringUtil::toString(config).c_str());
    std::string uuid = config.read("clusterUUID", std::string(""));
    if (!uuid.empty())
        clusterUUID.set(uuid);
}

ClientImpl::~ClientImpl()
{
    exactlyOnceRPCHelper.exit();
    eventLoop.exit();
    if (eventLoopThread.joinable())
        eventLoopThread.join();
}

void
ClientImpl::init(const std::string& hosts)
{
    this->hosts = hosts;
    eventLoopThread = std::thread(&Event::Loop::runForever, &eventLoop);
    initDerived();
}

void
ClientImpl::initDerived()
{
    if (!leaderRPC) { // sometimes set in unit tests
        NOTICE("Using server list: %s", hosts.c_str());
        leaderRPC.reset(new LeaderRPC(
            RPC::Address(hosts, Protocol::Common::DEFAULT_PORT),
            clusterUUID,
            sessionCreationBackoff,
            sessionManager));
    }
}

std::pair<uint64_t, Configuration>
ClientImpl::getConfiguration()
{
    // TODO(ongaro):  expose timeout
    Protocol::Client::GetConfiguration::Request request;
    Protocol::Client::GetConfiguration::Response response;
    leaderRPC->call(OpCode::GET_CONFIGURATION, request, response,
                    TimePoint::max());
    Configuration configuration;
    for (auto it = response.servers().begin();
         it != response.servers().end();
         ++it) {
        configuration.push_back({it->server_id(), it->addresses()});
    }
    return {response.id(), configuration};
}

ConfigurationResult
ClientImpl::setConfiguration(uint64_t oldId,
                             const Configuration& newConfiguration)
{
    // TODO(ongaro):  expose timeout
    Protocol::Client::SetConfiguration::Request request;
    request.set_old_id(oldId);
    for (auto it = newConfiguration.begin();
         it != newConfiguration.end();
         ++it) {
        Protocol::Client::Server* s = request.add_new_servers();
        s->set_server_id(it->serverId);
        s->set_addresses(it->addresses);
    }
    Protocol::Client::SetConfiguration::Response response;
    leaderRPC->call(OpCode::SET_CONFIGURATION, request, response,
                    TimePoint::max());
    ConfigurationResult result;
    if (response.has_ok()) {
        result.status = ConfigurationResult::OK;
        return result;
    }
    if (response.has_configuration_changed()) {
        result.status = ConfigurationResult::CHANGED;
        result.error = ("configuration changed: " +
                        response.configuration_changed().error());
        return result;
    }
    if (response.has_configuration_bad()) {
        result.status = ConfigurationResult::BAD;
        for (auto it = response.configuration_bad().bad_servers().begin();
             it != response.configuration_bad().bad_servers().end();
             ++it) {
            result.badServers.emplace_back(it->server_id(), it->addresses());
        }
        result.error = "servers slow or unavailable";
        return result;
    }
    PANIC("Did not understand server response to setConfiguration RPC:\n%s",
          Core::ProtoBuf::dumpString(response).c_str());
}

Result
ClientImpl::getServerInfo(const std::string& host,
                          TimePoint timeout,
                          Server& info)
{
    Result timeoutResult;
    timeoutResult.status = Client::Status::TIMEOUT;
    timeoutResult.error = "Client-specified timeout elapsed";

    while (true) {
        sessionCreationBackoff.delayAndBegin(timeout);

        RPC::Address address(host, Protocol::Common::DEFAULT_PORT);
        address.refresh(timeout);

        std::shared_ptr<RPC::ClientSession> session =
            sessionManager.createSession(address, timeout, &clusterUUID);

        Protocol::Client::GetServerInfo::Request request;
        RPC::ClientRPC rpc(session,
                           Protocol::Common::ServiceId::CLIENT_SERVICE,
                           1,
                           OpCode::GET_SERVER_INFO,
                           request);

        typedef RPC::ClientRPC::Status RPCStatus;
        Protocol::Client::GetServerInfo::Response response;
        Protocol::Client::Error error;
        RPCStatus status = rpc.waitForReply(&response, &error, timeout);

        // Decode the response
        switch (status) {
            case RPCStatus::OK:
                info.serverId = response.server_info().server_id();
                info.addresses = response.server_info().addresses();
                return Result();
            case RPCStatus::RPC_FAILED:
                break;
            case RPCStatus::TIMEOUT:
                return timeoutResult;
            case RPCStatus::SERVICE_SPECIFIC_ERROR:
                // Hmm, we don't know what this server is trying to tell us,
                // but something is wrong. The server shouldn't reply back with
                // error codes we don't understand. That's why we gave it a
                // serverSpecificErrorVersion number in the request header.
                PANIC("Unknown error code %u returned in service-specific "
                      "error. This probably indicates a bug in the server",
                      error.error_code());
                break;
            case RPCStatus::RPC_CANCELED:
                PANIC("RPC canceled unexpectedly");
            case RPCStatus::INVALID_SERVICE:
                PANIC("The server isn't running the ClientService");
            case RPCStatus::INVALID_REQUEST:
                PANIC("The server's ClientService doesn't support the "
                      "GetServerInfo RPC or claims the request is malformed");
        }
        if (timeout < Clock::now())
            return timeoutResult;
        else
            continue;
    }
}

Result
ClientImpl::write(const std::string& path,
                  const std::string& content,
                  TimePoint timeout)
{
    Protocol::Client::ReadWriteStore::Request request;
    *request.mutable_exactly_once() =
        exactlyOnceRPCHelper.getRPCInfo(timeout);
    request.mutable_write()->set_path(path);
    request.mutable_write()->set_content(content);
    Protocol::Client::ReadWriteStore::Response response;
    storeCall(*leaderRPC,
              request, response, timeout);
    exactlyOnceRPCHelper.doneWithRPC(request.exactly_once());
    if (response.status() != Protocol::Client::Status::OK)
        return storeError(response);
    return Result();
}

Result
ClientImpl::read(const std::string& path,
                 TimePoint timeout,
                 std::string& content)
{
    content.clear();
    Protocol::Client::ReadOnlyStore::Request request;
    request.mutable_read()->set_path(path);
    Protocol::Client::ReadOnlyStore::Response response;
    storeCall(*leaderRPC,
              request, response, timeout);
    if (response.status() != Protocol::Client::Status::OK)
        return storeError(response);
    content = response.read().content();
    return Result();
}

Result
ClientImpl::stat(const std::string& client,
                 TimePoint timeout,
                 std::string& content)
{
    content.clear();
    Protocol::Client::ReadOnlyStore::Request request;
    request.mutable_stat()->set_client(client);
    Protocol::Client::ReadOnlyStore::Response response;
    storeCall(*leaderRPC,
              request, response, timeout);
    if (response.status() != Protocol::Client::Status::OK)
        return storeError(response);
    content = response.stat().content();
    return Result();
}

Result
ClientImpl::range(const std::string& start_key,
                  const std::string& end_key,
                  uint64_t limit,
                  TimePoint timeout,
                  std::vector<std::string>& contents) {

    contents.clear();
    Protocol::Client::ReadOnlyStore::Request request;
    request.mutable_range()->set_start_key(start_key);
    request.mutable_range()->set_end_key(end_key);
    request.mutable_range()->set_limit(limit);
    Protocol::Client::ReadOnlyStore::Response response;
    storeCall(*leaderRPC,
              request, response, timeout);
    if (response.status() != Protocol::Client::Status::OK)
        return storeError(response);

    contents.reserve(response.range().contents_size());
    for (int i = 0; i < response.range().contents_size(); ++i) {
        contents.push_back(response.range().contents(i));
    }
    return Result();
}

Result
ClientImpl::search(const std::string& search_key,
                  uint64_t limit,
                  TimePoint timeout,
                  std::vector<std::string>& contents) {

    contents.clear();
    Protocol::Client::ReadOnlyStore::Request request;
    request.mutable_search()->set_search_key(search_key);
    request.mutable_search()->set_limit(limit);
    Protocol::Client::ReadOnlyStore::Response response;
    storeCall(*leaderRPC,
              request, response, timeout);
    if (response.status() != Protocol::Client::Status::OK)
        return storeError(response);

    contents.reserve(response.search().contents_size());
    for (int i = 0; i < response.search().contents_size(); ++i) {
        contents.push_back(response.search().contents(i));
    }
    return Result();
}


Result
ClientImpl::remove(const std::string& path,
                   TimePoint timeout)
{
    std::string realPath = path;
    Protocol::Client::ReadWriteStore::Request request;
    *request.mutable_exactly_once() =
        exactlyOnceRPCHelper.getRPCInfo(timeout);
    request.mutable_remove()->set_path(realPath);
    Protocol::Client::ReadWriteStore::Response response;
    storeCall(*leaderRPC,
              request, response, timeout);
    exactlyOnceRPCHelper.doneWithRPC(request.exactly_once());
    if (response.status() != Protocol::Client::Status::OK)
        return storeError(response);
    return Result();
}

Result
ClientImpl::serverControl(const std::string& host,
                          TimePoint timeout,
                          Protocol::ServerControl::OpCode opCode,
                          const google::protobuf::Message& request,
                          google::protobuf::Message& response)
{
    Result timeoutResult;
    timeoutResult.status = Client::Status::TIMEOUT;
    timeoutResult.error = "Client-specified timeout elapsed";

    while (true) {
        sessionCreationBackoff.delayAndBegin(timeout);

        RPC::Address address(host, Protocol::Common::DEFAULT_PORT);
        address.refresh(timeout);

        // TODO(ongaro): Ideally we'd learn the serverID the same way we learn
        // the cluster UUID and then assert that in future calls. In practice,
        // we're only making one call for now, so it doesn't matter.
        std::shared_ptr<RPC::ClientSession> session =
            sessionManager.createSession(address, timeout, &clusterUUID);

        RPC::ClientRPC rpc(session,
                           Protocol::Common::ServiceId::CONTROL_SERVICE,
                           1,
                           opCode,
                           request);

        typedef RPC::ClientRPC::Status RPCStatus;
        Protocol::Client::Error error;
        RPCStatus status = rpc.waitForReply(&response, &error, timeout);

        // Decode the response
        switch (status) {
            case RPCStatus::OK:
                return Result();
            case RPCStatus::RPC_FAILED:
                break;
            case RPCStatus::TIMEOUT:
                return timeoutResult;
            case RPCStatus::SERVICE_SPECIFIC_ERROR:
                // Hmm, we don't know what this server is trying to tell us,
                // but something is wrong. The server shouldn't reply back with
                // error codes we don't understand. That's why we gave it a
                // serverSpecificErrorVersion number in the request header.
                PANIC("Unknown error code %u returned in service-specific "
                      "error. This probably indicates a bug in the server",
                      error.error_code());
                break;
            case RPCStatus::RPC_CANCELED:
                PANIC("RPC canceled unexpectedly");
            case RPCStatus::INVALID_SERVICE:
                PANIC("The server isn't running the ControlService");
            case RPCStatus::INVALID_REQUEST:
                // ControlService was added in v1.1.0.
                EXIT("The server's ControlService doesn't support the "
                     "RPC or claims the request is malformed");
        }
        if (timeout < Clock::now())
            return timeoutResult;
        else
            continue;
    }
}


} // namespace LogCabin::Client
} // namespace LogCabin
