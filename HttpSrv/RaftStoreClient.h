#ifndef __RAFT_STORE_CLIENT_H__
#define __RAFT_STORE_CLIENT_H__

#include <LogCabin/Client.h>
#include <LogCabin/Debug.h>
#include <LogCabin/Util.h>

#include <boost/noncopyable.hpp>

namespace {

using LogCabin::Client::Cluster;
using LogCabin::Client::Store;
using LogCabin::Client::Util::parseNonNegativeDuration;

using LogCabin::Client::Result;
using LogCabin::Client::Status;

}

class RaftStoreClient: public boost::noncopyable {

public:
    static RaftStoreClient& Instance();
    bool init(const std::string& raftBackends);

    Result raft_set(const std::string& key, const std::string& val);
    Result raft_get(const std::string& key, std::string& val);
    Result raft_remove(const std::string& key);

    Result raft_range(const std::string& start_key, const std::string& end_key, uint64_t limit,
                      std::vector<std::string>& range_store);
    Result raft_search(const std::string& search_key, uint64_t limit,
                       std::vector<std::string>& search_store);

private:
    RaftStoreClient(){}
    ~RaftStoreClient() {}

    std::string raft_backends_;
    std::unique_ptr<Cluster> cluster_;
};


#endif // __RAFT_STORE_CLIENT_H__
