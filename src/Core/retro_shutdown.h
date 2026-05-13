#ifndef CP0_RETRO_SHUTDOWN_H
#define CP0_RETRO_SHUTDOWN_H

#include "libretro_core.h"

// Performs the core shutdown sequence and applies per-core unload workarounds.
void shutdown_retro_session(
    LibretroCore &core,
    const char *selected_env_core,
    const char *core_path
);

#endif // CP0_RETRO_SHUTDOWN_H
