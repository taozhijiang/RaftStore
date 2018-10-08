#include <tzhttpd/Log.h>
#include <tzhttpd/HttpServer.h>
#include <tzhttpd/CryptoUtil.h>

#include <json/json.h>

#include "RaftStoreClient.h"


namespace tzhttpd {

using namespace http_proto;

//simple http get method


//        "^/raftstore/api/.*/v1/remove$"
static
int check_api_v1_uri(const std::string& uri, const std::string& ops,
                  std::string& db) {
    if (uri.empty() || ops.empty()) {
        return -1;
    }

    std::vector<std::string> vec{};
    boost::split(vec, uri, boost::is_any_of("/?"));
    if (vec.size() < 6 ||
        vec[3].empty() ||
        vec[4] != "v1" ||
        vec[5] != ops ) {
        tzhttpd::tzhttpd_log_err("check ill uri for %s: %s", ops.c_str(), uri.c_str());
        return -2;
    }

    db = vec[3];
    return 0;
}


static
int raft_stat_handler(const HttpParser& http_parser,
                      std::string& response, std::string& status_line,
                      std::vector<std::string>& add_header) {

    Result result;
    std::string content;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        std::string client = params.VALUE("CLIENT");
        std::string stat;
        result = RaftStoreClient::Instance().raft_stat(client, stat);
        if (result.status == Status::OK) {
            content = std::move(stat);
        }

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"] = result.error;
    if (result.status == Status::OK) {
        root["VALUE"] = std::move(content);
    }

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raft_get_handler(const HttpParser& http_parser,
                     std::string& response, std::string& status_line,
                     std::vector<std::string>& add_header) {

    Result result;
    std::string content;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
        std::string dbname;
        if (check_api_v1_uri(Uri, "get", dbname) != 0) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        std::string key  = params.VALUE("KEY");
        std::string TYPE = params.VALUE("TYPE");
        std::string val;

        if (key.empty() || (!TYPE.empty() && TYPE != "base64")) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_get(dbname + "_" + key, val);
        if (result.status == Status::OK) {
            if (!TYPE.empty() && TYPE == "base64") {
                content = CryptoUtil::base64_encode(val);
            } else {
                if (!TYPE.empty()) {
                    tzhttpd_log_err("Unknown TYPE: %s", TYPE.c_str());
                }
                content = std::move(val);
            }
        }

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"] = result.error;
    if (result.status == Status::OK) {
        root["VALUE"] = std::move(content);
    }

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raft_set_handler(const HttpParser& http_parser,
                     std::string& response, std::string& status_line,
                     std::vector<std::string>& add_header) {

    Result result;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
        std::string dbname;
        if (check_api_v1_uri(Uri, "set", dbname) != 0) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        std::string key = params.VALUE("KEY");
        std::string val = params.VALUE("VALUE");

        if (key.empty() || val.empty()) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_set(dbname + "_" + key, val);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"]  = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raft_remove_handler(const HttpParser& http_parser,
                        std::string& response, std::string& status_line,
                        std::vector<std::string>& add_header) {

    Result result;
    std::string content;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
        std::string dbname;
        if (check_api_v1_uri(Uri, "remove", dbname) != 0) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        std::string key = params.VALUE("KEY");
        if(key.empty()) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_remove(dbname + "_" + key);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"]  = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}


static
int raft_range_handler(const HttpParser& http_parser,
                       std::string& response, std::string& status_line,
                       std::vector<std::string>& add_header) {

    Result result;
    Json::Value resultSets;
    std::vector<std::string> contents;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
    std::string dbname;
    if (check_api_v1_uri(Uri, "range", dbname) != 0) {
        result = Status::INVALID_ARGUMENT;
        goto ret;
    }

    do {

        std::string start_key = params.VALUE("START");
        std::string end_key = params.VALUE("END");
        auto limit_s = params.VALUE("LIMIT");
        uint64_t limit = limit_s.empty() ? 0 : ::atoll(limit_s.c_str());
        result = RaftStoreClient::Instance().raft_range(dbname + "_" + start_key,
                                                        end_key.empty() ? dbname + "~" : dbname + "_" + end_key,
                                                        limit, contents);

        const std::string key_prefix = dbname + "_";
        for (size_t i = 0; i< contents.size(); i++) {
            resultSets.append(contents[i].substr(key_prefix.size()));
        }

    } while (0);


ret:
    Json::Value root;
    root["CODE"]  = static_cast<int>(result.status);
    root["INFO"]  = result.error;
    if (!resultSets.empty()) {
        root["VALUE"] = Json::FastWriter().write(resultSets);
    }

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raft_search_handler(const HttpParser& http_parser,
                       std::string& response, std::string& status_line,
                       std::vector<std::string>& add_header) {

    Result result;
    Json::Value resultSets;
    std::vector<std::string> contents;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
    std::string dbname;
    if (check_api_v1_uri(Uri, "search", dbname) != 0) {
        result = Status::INVALID_ARGUMENT;
        goto ret;
    }

    do {

        std::string search_key = params.VALUE("SEARCH");
        auto limit_s = params.VALUE("LIMIT");
        uint64_t limit = limit_s.empty() ? 0 : ::atoll(limit_s.c_str());
        result = RaftStoreClient::Instance().raft_search(search_key, limit, contents);

        // 这里在应用层进行dbname的prefix过滤
        // 将搜索操作放到底层range搜索会更高效，目前使用这种简单的方式先实现功能
        const std::string key_prefix = dbname + "_";
        bool found = false;
        for (size_t i = 0; i< contents.size(); i++) {
            if(boost::algorithm::starts_with(contents[i], key_prefix)) {
                resultSets.append(contents[i].substr(key_prefix.size()));
                found = true;
                continue;
            }

            // key是排序返回的，所以如果前缀不满足了，则可以跳出循环了
            if(found == true)
                break;
        }

    } while (0);

ret:
    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"]  = result.error;
    if (!resultSets.empty()) {
        root["VALUE"] = Json::FastWriter().write(resultSets);
    }

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}



static
int raftpost_set_handler(const HttpParser& http_parser, const std::string& post_data,
                         std::string& response, std::string& status_line,
                         std::vector<std::string>& add_header) {

    Result result;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
        std::string dbname;
        if (check_api_v1_uri(Uri, "set", dbname) != 0) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(post_data, root) || root.isNull()) {
            tzhttpd_log_err("parse error for: %s", post_data.c_str());
            result = Result(Status::INVALID_ARGUMENT);
            break;
        }

        std::string TYPE = root["TYPE"].asString();
        std::string KEY  = root["KEY"].asString();
        std::string VALUE= root["VALUE"].asString();

        if (KEY.empty() || VALUE.empty() || (!TYPE.empty() && TYPE != "base64")) {
            tzhttpd_log_err("param error for: %s", post_data.c_str());
            result = Status::INVALID_ARGUMENT;
            break;
        }

        std::string val;
        if (TYPE == "base64") {
            val = CryptoUtil::base64_decode(VALUE);
        } else {
            val = std::move(VALUE);
        }

        result = RaftStoreClient::Instance().raft_set(KEY, val);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"] = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}


} // namespace httpd


//    ^/raftstore/api/[dbname]/v1/[ops]
//    将dbname嵌入到uri主要是方便使用HttpAuth进行数据库分组授权

bool raft_store_v1_http_init(std::shared_ptr<tzhttpd::HttpServer>& http_ptr) {

    //
    http_ptr->register_http_get_handler(
        "^/raftstore/api/v1/stat$", tzhttpd::raft_stat_handler, true);

    // KEY
    http_ptr->register_http_get_handler(
        "^/raftstore/api/.*/v1/get$", tzhttpd::raft_get_handler, true);
    // KEY, VALUE
    http_ptr->register_http_get_handler(
        "^/raftstore/api/.*/v1/set$", tzhttpd::raft_set_handler, true);
    // KEY
    http_ptr->register_http_get_handler(
        "^/raftstore/api/.*/v1/remove$", tzhttpd::raft_remove_handler, true);
    // START, END, LIMIT
    http_ptr->register_http_get_handler(
        "^/raftstore/api/.*/v1/range$", tzhttpd::raft_range_handler, true);
    // SEARCH, LIMIT
    http_ptr->register_http_get_handler(
        "^/raftstore/api/.*/v1/search$", tzhttpd::raft_search_handler, true);

    http_ptr->register_http_post_handler(
        "^/raftstore/api/.*/v1/set$", tzhttpd::raftpost_set_handler, true);

    return true;

}

