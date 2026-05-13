#ifndef CP0_ENV_UTILS_H
#define CP0_ENV_UTILS_H

// Parses common truthy/falsy environment variable values.
bool env_enabled_value(const char *env);

// Reads an unsigned integer environment variable, falling back on invalid input.
unsigned env_u32_or(const char *name, unsigned fallback);

#endif // CP0_ENV_UTILS_H
