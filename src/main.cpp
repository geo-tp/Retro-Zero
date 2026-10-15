#include "Core/retro_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv)
{
    // Flush every line only when explicitly collecting a crash diagnostic log.
    const char *debug_log = std::getenv("CP0_DEBUG_LOG");
    if (debug_log && std::strcmp(debug_log, "1") == 0) {
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
    }
    return run_retro_runtime(argc, argv);
}
