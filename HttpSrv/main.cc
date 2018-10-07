#include <syslog.h>
#include <cstdio>
#include <libconfig.h++>

#include <tzhttpd/CheckPoint.h>
#include <tzhttpd/Log.h>
#include <tzhttpd/HttpServer.h>

#include "RaftStoreClient.h"

namespace tzhttpd {
namespace http_handler {
extern std::string http_server_version;
}
} // end namespace tzhttpd

extern char * program_invocation_short_name;
static void usage() {
    std::stringstream ss;

    ss << program_invocation_short_name << ":" << std::endl;
    ss << "\t -c cfgFile  specify config file, default RaftStoreHttpSrv.conf " << std::endl;
    ss << "\t -d          daemonize service" << std::endl;
    ss << "\t -v          print version info" << std::endl;
    ss << std::endl;

    std::cout << ss.str();
}

char cfgFile[PATH_MAX] = "RaftStoreHttpSrv.conf";
bool daemonize = false;

extern bool raft_store_v1_http_init(std::shared_ptr<tzhttpd::HttpServer>& http_ptr);

int main(int argc, char* argv[]) {

    int opt_g = 0;
    while( (opt_g = getopt(argc, argv, "c:dhv")) != -1 ) {
        switch(opt_g)
        {
            case 'c':
                memset(cfgFile, 0, sizeof(cfgFile));
                strncpy(cfgFile, optarg, PATH_MAX);
                break;
            case 'd':
                daemonize = true;
                break;
            case 'v':
                std::cout << program_invocation_short_name << ": "
                    << tzhttpd::http_handler::http_server_version << std::endl;
                break;
            case 'h':
            default:
                usage();
                ::exit(EXIT_SUCCESS);
        }
    }

    std::cout << "using cfgfile: " << cfgFile << std::endl;
    libconfig::Config cfg;
    try {
        cfg.readFile(cfgFile);
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

        // daemonize should before any thread creation...
    if (daemonize) {
        std::cout << "We will daemonize this service..." << std::endl;
        tzhttpd::tzhttpd_log_notice("We will daemonize this service...");

        bool chdir = false; // leave the current working directory in case
                            // the user has specified relative paths for
                            // the config file, etc
        bool close = true;  // close stdin, stdout, stderr
        if (::daemon(!chdir, !close) != 0) {
            tzhttpd::tzhttpd_log_err("Call to daemon() failed: %s", strerror(errno));
            std::cout << "Call to daemon() failed: " << strerror(errno) << std::endl;
            ::exit(EXIT_FAILURE);
        }
    }

    std::shared_ptr<tzhttpd::HttpServer> http_server_ptr;
    http_server_ptr.reset(new tzhttpd::HttpServer(cfgFile, program_invocation_short_name));
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
