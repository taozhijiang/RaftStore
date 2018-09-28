#include <tzhttpd/Log.h>
#include <tzhttpd/HttpServer.h>
#include <tzhttpd/CryptoUtil.h>

#include <json/json.h>

#include "RaftStoreClient.h"


namespace tzhttpd {

using namespace http_proto;

//simple http get method

static
int raft_get_handler(const HttpParser& http_parser,
                     std::string& response, std::string& status_line,
                     std::vector<std::string>& add_header) {

    Result result;
    std::string content;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        if (params.VALUE("AUTH") != "d44bfc666db304b2f72b4918c8b46f78") {
            tzhttpd_log_err("AUTH check failed!");
            response    = http_proto::content_forbidden;
            status_line = generate_response_status_line(http_parser.get_version(),
                                                    StatusCode::client_error_forbidden);
            return 0;
        }

        std::string key  = params.VALUE("KEY");
        std::string val;
        result = RaftStoreClient::Instance().raft_get(key, val);
        if (result.status == Status::OK) {
            content = std::move(val);
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
    add_header  = { "Cache-Control: no-cache", "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raft_set_handler(const HttpParser& http_parser,
                     std::string& response, std::string& status_line,
                     std::vector<std::string>& add_header) {

    Result result;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        if (params.VALUE("AUTH") != "d44bfc666db304b2f72b4918c8b46f78") {
            tzhttpd_log_err("AUTH check failed!");
            response = http_proto::content_forbidden;
            status_line = generate_response_status_line(http_parser.get_version(),
                                                    StatusCode::client_error_forbidden);
            return 0;
        }

        std::string key = params.VALUE("KEY");
        std::string val = params.VALUE("VALUE");
        result = RaftStoreClient::Instance().raft_set(key, val);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"]  = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache", "Content-type: application/json; charset=utf-8;"};

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

        if (params.VALUE("AUTH") != "d44bfc666db304b2f72b4918c8b46f78") {
            tzhttpd_log_err("AUTH check failed!");
            response = http_proto::content_forbidden;
            status_line = generate_response_status_line(http_parser.get_version(),
                                                    StatusCode::client_error_forbidden);
            return 0;
        }

        std::string key = params.VALUE("KEY");
        result = RaftStoreClient::Instance().raft_remove(key);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"]  = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache", "Content-type: application/json; charset=utf-8;"};

    return 0;
}



// http post

static
int raftpost_get_handler(const HttpParser& http_parser, const std::string& post_data,
                         std::string& response, std::string& status_line,
                         std::vector<std::string>& add_header) {

    Result result;
    std::string content;

    do {

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(post_data, root) || root.isNull()) {
            tzhttpd_log_err("parse error for: %s", post_data.c_str());
            result = Result(Status::INVALID_ARGUMENT);
            break;
        }

        std::string TYPE = root["TYPE"].asString();
        std::string KEY  = root["KEY"].asString();
        std::string AUTH = root["AUTH"].asString();
        if (AUTH != "d44bfc666db304b2f72b4918c8b46f78") {
            tzhttpd_log_err("AUTH check failed!");
            response = http_proto::content_forbidden;
            status_line = generate_response_status_line(http_parser.get_version(),
                                                    StatusCode::client_error_forbidden);
            return 0;
        }

        if (KEY.empty() || (!TYPE.empty() && TYPE != "base64")) {
            tzhttpd_log_err("param error for: %s", post_data.c_str());
            result = Result(Status::INVALID_ARGUMENT);
            break;
        }

        std::string key;
        std::string val;
        if (TYPE == "base64")
            key = CryptoUtil::base64_decode(KEY);
        else
            key = KEY;

        result = RaftStoreClient::Instance().raft_get(key, val);
        if (result.status == Status::OK) {
            if (TYPE == "base64")
                content = CryptoUtil::base64_encode(val);
            else
                content = std::move(val);
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
    add_header  = { "Cache-Control: no-cache", "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raftpost_set_handler(const HttpParser& http_parser, const std::string& post_data,
                         std::string& response, std::string& status_line,
                         std::vector<std::string>& add_header) {

    Result result;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

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
        std::string AUTH = root["AUTH"].asString();
        if (AUTH != "d44bfc666db304b2f72b4918c8b46f78") {
            tzhttpd_log_err("AUTH check failed!");
            response = http_proto::content_forbidden;
            status_line = generate_response_status_line(http_parser.get_version(),
                                                    StatusCode::client_error_forbidden);
            return 0;
        }

        if (KEY.empty() || VALUE.empty() || (!TYPE.empty() && TYPE != "base64")) {
            tzhttpd_log_err("param error for: %s", post_data.c_str());
            result = Result(Status::INVALID_ARGUMENT);
            break;
        }

        std::string key;
        std::string val;
        if (TYPE == "base64") {
            key = CryptoUtil::base64_decode(KEY);
            val = CryptoUtil::base64_decode(VALUE);
        } else {
            key = std::move(KEY);
            val = std::move(VALUE);
        }

        result = RaftStoreClient::Instance().raft_set(key, val);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"] = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache", "Content-type: application/json; charset=utf-8;"};

    return 0;
}

static
int raftpost_remove_handler(const HttpParser& http_parser, const std::string& post_data,
                            std::string& response, std::string& status_line,
                            std::vector<std::string>& add_header) {

    Result result;
    std::string content;
    const UriParamContainer& params = http_parser.get_request_uri_params();

    do {

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(post_data, root) || root.isNull()) {
            tzhttpd_log_err("parse error for: %s", post_data.c_str());
            result = Result(Status::INVALID_ARGUMENT);
            break;
        }

        std::string TYPE = root["TYPE"].asString();
        std::string KEY  = root["KEY"].asString();
        std::string AUTH = root["AUTH"].asString();
        if (AUTH != "d44bfc666db304b2f72b4918c8b46f78") {
            tzhttpd_log_err("AUTH check failed!");
            response = http_proto::content_forbidden;
            status_line = generate_response_status_line(http_parser.get_version(),
                                                    StatusCode::client_error_forbidden);
            return 0;
        }

        if (KEY.empty() || (!TYPE.empty() && TYPE != "base64")) {
            tzhttpd_log_err("param error for: %s", post_data.c_str());
            result = Result(Status::INVALID_ARGUMENT);
            break;
        }

        std::string key;
        if (TYPE == "base64")
            key = CryptoUtil::base64_decode(KEY);
        else
            key = std::move(KEY);

        result = RaftStoreClient::Instance().raft_remove(key);

    } while (0);

    Json::Value root;
    root["CODE"] = static_cast<int>(result.status);
    root["INFO"] = result.error;

    response    = Json::FastWriter().write(root);
    status_line = http_proto::generate_response_status_line(
                        http_parser.get_version(), StatusCode::success_ok);
    add_header  = { "Cache-Control: no-cache", "Content-type: application/json; charset=utf-8;"};

    return 0;
}


} // namespace httpd


bool raft_store_v1_http_init(std::shared_ptr<tzhttpd::HttpServer>& http_ptr) {

    http_ptr->register_http_get_handler(
        "^/raftstore/api/v1/get$", tzhttpd::raft_get_handler, true);
    http_ptr->register_http_get_handler(
        "^/raftstore/api/v1/set$", tzhttpd::raft_set_handler, true);
    http_ptr->register_http_get_handler(
        "^/raftstore/api/v1/remove$", tzhttpd::raft_remove_handler, true);

    http_ptr->register_http_post_handler(
        "^/raftstore/api/v1/get$", tzhttpd::raftpost_get_handler, true);
    http_ptr->register_http_post_handler(
        "^/raftstore/api/v1/set$", tzhttpd::raftpost_set_handler, true);
    http_ptr->register_http_post_handler(
        "^/raftstore/api/v1/remove$", tzhttpd::raftpost_remove_handler, true);

    return true;

}

