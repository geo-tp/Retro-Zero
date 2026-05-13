#ifndef CP0_CORE_DOWNLOADER_H
#define CP0_CORE_DOWNLOADER_H

#include "core_config.h"

#include <string>

namespace CoreDownloader {

// Downloads and extracts the configured Libretro core when it is missing locally.
bool downloadForConsole(const CoreConfig &console, std::string &error);

// Returns the installed core path when present, or an empty string otherwise.
std::string resolveInstalledCore(const CoreConfig &console);

} // namespace CoreDownloader

#endif // CP0_CORE_DOWNLOADER_H
