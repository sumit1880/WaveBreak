#pragma once

#include "wavebreak/common.hpp"
#include <string>
#include <vector>

namespace wavebreak {

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

} // namespace wavebreak
