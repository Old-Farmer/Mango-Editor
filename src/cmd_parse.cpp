#include "cmd_parse.h"

#include <getopt.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace mango {

namespace {

constexpr const char* KUsageTemplate =
    "Usage:\n"
    "  mgo [options] [file ...]\n"
    "\n"
    "Options:\n"
    "   -h(--help) Show usage";

void PrintUsage(int status = EXIT_SUCCESS) {
    puts(KUsageTemplate);
    exit(status);
}

}  // namespace

void ParseCmdArgs(int argc, char* argv[], Options* options,
                  InitOptions* init_options) {
    const char* short_opts = "h";
    const option long_opts[] = {
        {"help", no_argument, nullptr, 'h'},
    };

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, nullptr)) !=
           -1) {
        switch (c) {
            case 'h': {
                PrintUsage();
                break;
            }
            case '?': {
                PrintUsage(EXIT_FAILURE);
                break;
            }
        }
    }

    while (optind < argc) {
        init_options->begin_files.push_back(argv[optind++]);
    }
    for (auto file : init_options->begin_files) {
        MGO_LOG_DEBUG("cmd args: file name: %s", file);
    }
    (void)options;
}

}  // namespace mango