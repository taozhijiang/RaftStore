#include <tzhttpd/Log.h>

#include "RaftStoreClient.h"

RaftStoreClient& RaftStoreClient::Instance() {
    static RaftStoreClient helper{};
    return helper;
}

bool RaftStoreClient::init(const std::string& raftBackends) {

    if (cluster_) {
        tzhttpd::tzhttpd_log_err("Cluster may already initialized with: %s",
                                 raft_backends_.c_str());
        return false;
    }

    raft_backends_ = raftBackends;
    cluster_.reset(new Cluster(raft_backends_));

    return cluster_ ? true : false;
}

Result RaftStoreClient::raft_stat(const std::string& client, std::string& stat) {

    if (!cluster_) {
        tzhttpd::tzhttpd_log_err("param error");
        return Status::INVALID_ARGUMENT;
    }

    auto store = cluster_->getStore();
    auto result = store.stat(client, stat);
    if(result.status != Status::OK) {
        tzhttpd::tzhttpd_log_err("stat(%s) error with: %d(%s)",
                                 client.c_str(),
                                 result.status, result.error.c_str());
        return result;
    }

    tzhttpd::tzhttpd_log_debug("stat(%s) ok!", client.c_str());
    return result;
}


Result RaftStoreClient::raft_set(const std::string& key, const std::string& val) {

    if (key.empty() || val.empty() || !cluster_) {
        tzhttpd::tzhttpd_log_err("param error");
        return Status::INVALID_ARGUMENT;
    }

    auto store = cluster_->getStore();
    auto result = store.write(key, val);
    if(result.status != Status::OK) {
        tzhttpd::tzhttpd_log_err("write(%s:%s) error with: %d(%s)",
                                 key.c_str(), val.c_str(),
                                 result.status, result.error.c_str());
        return result;
    }

    tzhttpd::tzhttpd_log_debug("write(%s:%s) ok!", key.c_str(), val.c_str());
    return result;
}

Result RaftStoreClient::raft_get(const std::string& key, std::string& val) {

    if (key.empty() || !cluster_) {
        tzhttpd::tzhttpd_log_err("param error");
        return Status::INVALID_ARGUMENT;
    }

    auto store = cluster_->getStore();
    auto result = store.read(key, val);
    if(result.status != Status::OK) {
        tzhttpd::tzhttpd_log_err("read(%s) error with: %d(%s)",
                                 key.c_str(),
                                 result.status, result.error.c_str());
        return result;
    }

    tzhttpd::tzhttpd_log_debug("read(%s) ok!", key.c_str());
    return result;
}

Result RaftStoreClient::raft_remove(const std::string& key) {
    if (key.empty() || !cluster_) {
        tzhttpd::tzhttpd_log_err("param error");
        return Status::INVALID_ARGUMENT;
    }

    auto store = cluster_->getStore();
    auto result = store.remove(key);
    if(result.status != Status::OK) {
        tzhttpd::tzhttpd_log_err("remove(%s) error with: %d(%s)",
                                 key.c_str(),
                                 result.status, result.error.c_str());
        return result;
    }

    tzhttpd::tzhttpd_log_debug("remove(%s) ok!", key.c_str());
    return result;
}


Result RaftStoreClient::raft_range(const std::string& start_key, const std::string& end_key, uint64_t limit,
                                   std::vector<std::string>& range_store) {

    if (!cluster_) {
        tzhttpd::tzhttpd_log_err("param error");
        return Status::INVALID_ARGUMENT;
    }

    auto store = cluster_->getStore();
    auto result = store.range(start_key, end_key, limit, range_store);
    if(result.status != Status::OK) {
        tzhttpd::tzhttpd_log_err("range(%s:%s) error with: %d(%s)",
                                 start_key.c_str(), end_key.c_str(),
                                 result.status, result.error.c_str());
        return result;
    }

    tzhttpd::tzhttpd_log_debug("range(%s:%s) ok!", start_key.c_str(),
                               end_key.c_str());
    return result;
}

Result RaftStoreClient::raft_search(const std::string& search_key, uint64_t limit,
                                    std::vector<std::string>& search_store) {

    if (search_key.empty() || !cluster_) {
        tzhttpd::tzhttpd_log_err("param error");
        return Status::INVALID_ARGUMENT;
    }

    auto store = cluster_->getStore();
    auto result = store.search(search_key, limit, search_store);
    if(result.status != Status::OK) {
        tzhttpd::tzhttpd_log_err("search(%s) error with: %d(%s)",
                                 search_key.c_str(),
                                 result.status, result.error.c_str());
        return result;
    }

    tzhttpd::tzhttpd_log_debug("range(%s) ok!", search_key.c_str());
    return result;
}


