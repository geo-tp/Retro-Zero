#include "env_utils.h"

#include <cstdlib>
#include <cstring>

// Parses boolean-like environment values used by feature toggles.
bool env_enabled_value(const char *env)
{
    if (!env || !env[0]) {
        return false;
    }

    return std::strcmp(env, "0") != 0 &&
           std::strcmp(env, "false") != 0 &&
           std::strcmp(env, "False") != 0 &&
           std::strcmp(env, "FALSE") != 0 &&
           std::strcmp(env, "disabled") != 0 &&
           std::strcmp(env, "Disabled") != 0 &&
           std::strcmp(env, "DISABLED") != 0 &&
           std::strcmp(env, "off") != 0 &&
           std::strcmp(env, "Off") != 0 &&
           std::strcmp(env, "OFF") != 0 &&
           std::strcmp(env, "no") != 0 &&
           std::strcmp(env, "No") != 0 &&
           std::strcmp(env, "NO") != 0;
}

// Parses unsigned integer environment values with a safe fallback.
unsigned env_u32_or(const char *name, unsigned fallback)
{
    const char *value = std::getenv(name);
    if (!value || !value[0]) {
        return fallback;
    }

    char *end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || parsed == 0 || parsed > 4096) {
        return fallback;
    }

    return static_cast<unsigned>(parsed);
}
