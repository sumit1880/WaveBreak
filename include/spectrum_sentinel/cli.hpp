#pragma once

#include "spectrum_sentinel/common.hpp"
#include <string>
#include <vector>

namespace spectrum_sentinel {

enum class ExitCode : int {
    Success = 0,
    RuntimeError = 1,
    UsageError = 2,
    FileIOError = 3
};

class CLI {
public:
    static int run(int argc, char* argv[]);

    static void print_usage();
    static void print_version();
};

} // namespace spectrum_sentinel
