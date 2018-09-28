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

