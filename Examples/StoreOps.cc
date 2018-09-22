/* Copyright (c) 2012 Stanford University
 * Copyright (c) 2015 Diego Ongaro
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

#include <cassert>
#include <getopt.h>
#include <iostream>
#include <iterator>
#include <sstream>

#include <LogCabin/Client.h>
#include <LogCabin/Debug.h>
#include <LogCabin/Util.h>

namespace {

using LogCabin::Client::Cluster;
using LogCabin::Client::Store;
using LogCabin::Client::Util::parseNonNegativeDuration;

using LogCabin::Client::Result;
using LogCabin::Client::Status;

enum class Command {
    WRITE,
    READ,
    REMOVE,
};

/**
 * Parses argv for the main function.
 */
class OptionParser {
  public:
    OptionParser(int& argc, char**& argv)
        : argc(argc)
        , argv(argv)
        , cluster("logcabin:5254")
        , command()
        , logPolicy("")
        , path()
        , timeout(parseNonNegativeDuration("0s"))
    {
        while (true) {
            static struct option longOptions[] = {
               {"cluster",  required_argument, NULL, 'c'},
               {"help",  no_argument, NULL, 'h'},
               {"quiet",  no_argument, NULL, 'q'},
               {"timeout",  required_argument, NULL, 't'},
               {"verbose",  no_argument, NULL, 'v'},
               {"verbosity",  required_argument, NULL, 256},
               {0, 0, 0, 0}
            };
            int c = getopt_long(argc, argv, "c:d:p:t:hqv", longOptions, NULL);

            // Detect the end of the options.
            if (c == -1)
                break;

            switch (c) {
                case 'c':
                    cluster = optarg;
                    break;
                case 'h':
                    usage();
                    exit(0);
                case 'q':
                    logPolicy = "WARNING";
                    break;
                case 't':
                    timeout = parseNonNegativeDuration(optarg);
                    break;
                case 'v':
                    logPolicy = "VERBOSE";
                    break;
                case 256:
                    logPolicy = optarg;
                    break;
                case '?':
                default:
                    // getopt_long already printed an error message.
                    usage();
                    exit(1);
            }
        }

        // Additional command line arguments are required.
        if (optind == argc) {
            usage();
            exit(1);
        }

        std::string cmdStr = argv[optind];
        ++optind;
        if (cmdStr == "write" || cmdStr == "create" || cmdStr == "set") {
            command = Command::WRITE;
        } else if (cmdStr == "read" || cmdStr == "get") {
            command = Command::READ;
        } else if (cmdStr == "remove" || cmdStr == "rm") {
            command = Command::REMOVE;
        } else {
            std::cout << "Unknown command: " << cmdStr << std::endl;
            usage();
            exit(1);
        }

        if (optind < argc) {
            path = argv[optind];
            ++optind;
        }

        if (path.empty()) {
            std::cout << "No path given" << std::endl;
            usage();
            exit(1);
        }
        if (optind < argc) {
            std::cout << "Unexpected positional argument: " << argv[optind]
                      << std::endl;
            usage();
            exit(1);
        }
    }

    void usage() {
        std::cout << "Run various operations on a LogCabin replicated state "
                  << "machine."
                  << std::endl
                  << std::endl
                  << "This program was released in LogCabin v1.0.0."
                  << std::endl;
        std::cout << std::endl;

        std::cout << "Usage: " << argv[0] << " [options] <command> [<args>]"
                  << std::endl;
        std::cout << std::endl;

        std::cout << "Commands:" << std::endl;
        std::cout
            << "  write <path>    Set/create value of file at <path> to "
            << "stdin."
            << std::endl
            << "                  Alias: create, set."
            << std::endl
            << "  read <path>     Print value of file at <path>. Alias: get."
            << std::endl
            << "  remove <path>   Remove file at <path>, if any. Alias: rm. "
            << std::endl
            << std::endl;

        std::cout << "Options:" << std::endl;
        std::cout
            << "  -c <addresses>, --cluster=<addresses>  "
            << "Network addresses of the LogCabin"
            << std::endl
            << "                                         "
            << "servers, comma-separated"
            << std::endl
            << "                                         "
            << "[default: logcabin:5254]"
            << std::endl

            << "  -h, --help                     "
            << "Print this usage information"
            << std::endl

            << "  -q, --quiet                    "
            << "Same as --verbosity=WARNING"
            << std::endl

            << "  -t <time>, --timeout=<time>    "
            << "Set timeout for the operation"
            << std::endl
            << "                                 "
            << "(0 means wait forever) [default: 0s]"
            << std::endl

            << "  -v, --verbose                  "
            << "Same as --verbosity=VERBOSE (added in v1.1.0)"
            << std::endl

            << "  --verbosity=<policy>           "
            << "Set which log messages are shown."
            << std::endl
            << "                                 "
            << "Comma-separated LEVEL or PATTERN@LEVEL rules."
            << std::endl
            << "                                 "
            << "Levels: SILENT ERROR WARNING NOTICE VERBOSE."
            << std::endl
            << "                                 "
            << "Patterns match filename prefixes or suffixes."
            << std::endl
            << "                                 "
            << "Example: Client@NOTICE,Test.cc@SILENT,VERBOSE."
            << std::endl
            << "                                 "
            << "(added in v1.1.0)"
            << std::endl;
    }

    int& argc;
    char**& argv;
    std::string cluster;
    Command command;
    std::pair<std::string, std::string> condition;
    std::string dir;
    std::string logPolicy;
    std::string path;
    uint64_t timeout;
};

std::string
readStdin()
{
    std::cin >> std::noskipws;
    std::istream_iterator<char> it(std::cin);
    std::istream_iterator<char> end;
    std::string results(it, end);
    return results;
}

} // anonymous namespace

int
main(int argc, char** argv)
{
    OptionParser options(argc, argv);

    LogCabin::Client::Debug::setLogPolicy(
        LogCabin::Client::Debug::logPolicyFromString(
            options.logPolicy));

    Cluster cluster(options.cluster);
    Store store = cluster.getStore();

    if (options.timeout > 0) {
        store.setTimeout(options.timeout);
    }

    std::string& path = options.path;
    Result result {};
    switch (options.command) {
        case Command::WRITE:
            result = store.write(path, readStdin());
            if(result.status != Status::OK) {
                fprintf(stderr, "write error with: %d(%s)\n", result.status, result.error.c_str());
            }
            break;
        case Command::READ: {
            std::string contents;
            result = store.read(path, contents);
            if(result.status != Status::OK) {
                fprintf(stderr, "read error with: %d(%s)\n", result.status, result.error.c_str());
                break;
            }
            std::cout << contents;
            if (contents.empty() ||
                contents.at(contents.size() - 1) != '\n') {
                std::cout << std::endl;
            } else {
                std::cout.flush();
            }
            break;
        }
        case Command::REMOVE:
            result = store.remove(path);
            if(result.status != Status::OK) {
                fprintf(stderr, "remove error with: %d(%s)\n", result.status, result.error.c_str());
                break;
            }
            break;
    }
    return 0;
}
