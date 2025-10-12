#pragma once

#include "options.h"

namespace mango {

// Parse command line arguments and resuls will be written in options.
// If some error occurs or -h(--help) is in args, this function just calls exit.
void ParseCmdArgs(int args, char* argv[], Options& options);

}