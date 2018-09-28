#include <syslog.h>
#include <cstdio>
#include <libconfig.h++>

#include <tzhttpd/CheckPoint.h>
#include <tzhttpd/Log.h>
#include <tzhttpd/HttpServer.h>

#include "RaftStoreClient.h"

extern bool raft_store_v1_http_init(std::shared_ptr<tzhttpd::HttpServer>& http_ptr);

int main(int argc, char* argv[]) {

    std::string cfgfile = std::string(program_invocation_short_name) + ".conf";
    std::cout << "using cfgfile: " << cfgfile << std::endl;
    libconfig::Config cfg;
    try {
        cfg.readFile(cfgfile.c_str());
    } catch(libconfig::FileIOException &fioex) {
        fprintf(stderr, "I/O error while reading file.");
        return -1;
    } catch(libconfig::ParseException &pex) {
        fprintf(stderr, "Parse error at %d - %s", pex.getLine(), pex.getError());
        return -1;
    }

    // default syslog
    tzhttpd::set_checkpoint_log_store_func(::syslog);
    tzhttpd::tzhttpd_log_init(7);

    std::shared_ptr<tzhttpd::HttpServer> http_server_ptr;
    http_server_ptr.reset(new tzhttpd::HttpServer(cfgfile, program_invocation_short_name));
    if (!http_server_ptr || !http_server_ptr->init()) {
        fprintf(stderr, "Init HttpServer failed!");
        return -1;
    }

    std::string hosts;
    cfg.lookupValue("Raft.BackendHosts", hosts);
    if (hosts.empty() || !RaftStoreClient::Instance().init(hosts)) {
        tzhttpd::tzhttpd_log_notice("Init RaftStoreClient failed.");
        return -1;
    }

    if (!raft_store_v1_http_init(http_server_ptr)) {
        tzhttpd::tzhttpd_log_notice("add raft_store http handler failed.");
        return -1;
    }


    fprintf(stderr, "tzhttpd service begin to run ...");
    tzhttpd::tzhttpd_log_notice("tzhttpd service begin to run ...");

    http_server_ptr->io_service_threads_.start_threads();
    http_server_ptr->service();

    http_server_ptr->io_service_threads_.join_threads();

    return 0;
}

namespace boost {

void assertion_failed(char const * expr, char const * function, char const * file, long line) {
    fprintf(stderr, "BAD!!! expr `%s` assert failed at %s(%ld): %s", expr, file, line, function);
}

} // end boost
