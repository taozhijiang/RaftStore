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

        std::string CLIENT = params.VALUE("client");
        std::string stat;
        result = RaftStoreClient::Instance().raft_stat(CLIENT, stat);
        if (result.status == Status::OK) {
            content = std::move(stat);
        }

    } while (0);

    Json::Value root;
    root["code"] = static_cast<int>(result.status);
    root["info"] = result.error;
    if (result.status == Status::OK) {
        root["value"] = std::move(content);
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
    // compact: 先使用deflate压缩，然后使用base64编码，数据放在json当中
    // raw:     原始数据直接返回，而且会根据文件名添加application header
    // deflate: 数据使用deflate压缩返回，而且会根据文件名添加application header
    std::string TYPE = params.VALUE("type");
    std::string KEY  = params.VALUE("key");

    do {

        std::string Uri = http_parser.find_request_header(http_proto::header_options::request_path_info);
        std::string dbname;
        if (check_api_v1_uri(Uri, "get", dbname) != 0) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        std::string val;
        if (KEY.empty() || (!TYPE.empty() && TYPE != "compact" && TYPE != "raw" && TYPE != "deflate")) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_get(dbname + "_" + KEY, val);
        if (result.status == Status::OK) {
            if (TYPE == "compact") {
                content = CryptoUtil::base64_encode(CryptoUtil::Deflator(val));
            } else if (TYPE == "deflate") {
                content = CryptoUtil::Deflator(val);
                add_header.push_back("Content-Encoding: deflate");
            } else {
                content = std::move(val);
            }
        }

    } while (0);

    // 对于raw类型的，只有成功才会返回数据，否则返回错误的HTTP信息
    if (TYPE == "raw" || TYPE == "deflate") {

        if (result.status != Status::OK) {
            status_line = http_proto::generate_response_status_line(
                                http_parser.get_version(), StatusCode::server_error_internal_server_error);
            return 0;
        }

        response = std::move(content);

        std::string cp_path = KEY;
        boost::to_lower(cp_path);
        // 取出扩展名
        std::string::size_type pos = cp_path.rfind(".");
        std::string suffix {};
        if (pos != std::string::npos && (cp_path.size() - pos) < 6) {
            suffix = cp_path.substr(pos);
        }

        if (!suffix.empty()) {

            std::string content_type = http_proto::find_content_type(suffix);
            if (!content_type.empty()) {
                add_header.push_back(content_type);
                tzhttpd_log_debug("Adding content_type header for %s(%s) -> %s",
                                  cp_path.c_str(), suffix.c_str(), content_type.c_str());
            }
        }

    } else {

        Json::Value root;
        root["code"] = static_cast<int>(result.status);
        root["info"] = result.error;
        if (result.status == Status::OK) {
            root["value"] = std::move(content);
        }

        response    = Json::FastWriter().write(root);
        add_header  = { "Cache-Control: no-cache",
                        "Content-type: application/json; charset=utf-8;"};
    }

    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);

    return 0;
}

// 只处理VALUE是简单字符的情况，VALUE会自动被urlencode
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

        std::string KEY   = params.VALUE("key");
        std::string VALUE = params.VALUE("value");

        if (KEY.empty() || VALUE.empty()) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_set(dbname + "_" + KEY, VALUE);

    } while (0);

    Json::Value root;
    root["code"] = static_cast<int>(result.status);
    root["info"]  = result.error;

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

        std::string key = params.VALUE("key");
        if(key.empty()) {
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_remove(dbname + "_" + key);

    } while (0);

    Json::Value root;
    root["code"] = static_cast<int>(result.status);
    root["info"]  = result.error;

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

        std::string START_KEY = params.VALUE("start");
        std::string END_KEY   = params.VALUE("end");
        auto limit_s = params.VALUE("limit");
        uint64_t LIMIT = limit_s.empty() ? 0 : ::atoll(limit_s.c_str());
        result = RaftStoreClient::Instance().raft_range(dbname + "_" + START_KEY,
                                                        END_KEY.empty() ? dbname + "~" : dbname + "_" + END_KEY,
                                                        LIMIT, contents);

        const std::string key_prefix = dbname + "_";
        for (size_t i = 0; i< contents.size(); i++) {
            resultSets.append(contents[i].substr(key_prefix.size()));
        }

    } while (0);


ret:
    Json::Value root;
    root["code"]  = static_cast<int>(result.status);
    root["info"]  = result.error;
    if (!resultSets.empty()) {
        root["value"] = Json::FastWriter().write(resultSets);
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

        std::string SEARCH_KEY = params.VALUE("search");
        auto limit_s = params.VALUE("limit");
        uint64_t LIMIT = limit_s.empty() ? 0 : ::atoll(limit_s.c_str());
        result = RaftStoreClient::Instance().raft_search(SEARCH_KEY, LIMIT, contents);

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
    root["code"] = static_cast<int>(result.status);
    root["info"]  = result.error;
    if (!resultSets.empty()) {
        root["value"] = Json::FastWriter().write(resultSets);
    }

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache",
                    "Content-type: application/json; charset=utf-8;"};

    return 0;
}


// POST方式，处理复杂的数据上传请求
static
int raftpost_set_handler(const HttpParser& http_parser, const std::string& post_data,
                         std::string& response, std::string& status_line,
                         std::vector<std::string>& add_header) {

    Result result;
    const UriParamContainer& params = http_parser.get_request_uri_params();
    std::string md5sum;

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

        std::string TYPE   = root["type"].asString();
        std::string KEY    = root["key"].asString();
        std::string VALUE  = root["value"].asString();
        std::string MD5SUM = root["md5sum"].asString();

        if (KEY.empty() || VALUE.empty() || MD5SUM.empty() || (!TYPE.empty() && TYPE != "compact")) {
            tzhttpd_log_err("param error for: %s", post_data.c_str());
            result = Status::INVALID_ARGUMENT;
            break;
        }

        std::string val;
        if (TYPE == "compact") {
            val = CryptoUtil::Inflator(CryptoUtil::base64_decode(VALUE));
        } else {
            val = std::move(VALUE);
        }

        if (val.empty()) {
            tzhttpd_log_err("final empty value for: %s", post_data.c_str());
            result = Status::INVALID_ARGUMENT;
            break;
        }

        md5sum = CryptoUtil::to_hex_string(CryptoUtil::md5(val));
        if (!boost::iequals(md5sum, MD5SUM)) {
            tzhttpd_log_err("data MD5SUM check error, expect:%s, get:%s",
                            MD5SUM.c_str(), md5sum.c_str());
            result = Status::INVALID_ARGUMENT;
            break;
        }

        result = RaftStoreClient::Instance().raft_set(dbname + '_' + KEY, val);

    } while (0);

    Json::Value root;
    root["code"] = static_cast<int>(result.status);
    root["info"] = result.error;
    if(result.status == Status::OK && !md5sum.empty()) {
        root["md5sum"] = MD5SUM;  // 返回原始传来的md5，防止大小写问题
    }

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

