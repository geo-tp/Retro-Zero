#include "core_downloader.h"
#include "../Utils/bios_utils.h"
#include "../Utils/file_utils.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string shell_quote(const std::string &s)
{
    std::string q = "'";
    for (char c : s) {
        if (c == '\'') q += "'\\''";
        else q += c;
    }
    q += "'";
    return q;
}

} // namespace

namespace CoreDownloader {

// Resolves the local .so path when a core is already installed.
std::string resolveInstalledCore(const CoreConfig &console)
{
    const char *env = console.envCore ? std::getenv(console.envCore) : nullptr;
    std::vector<std::string> candidates;

    if (env && env[0]) candidates.push_back(env);
    if (console.corePath && console.corePath[0]) candidates.push_back(console.corePath);

    if (console.envCore && std::strcmp(console.envCore, "CP0_CORE_NEOGEO") == 0) {
        candidates.push_back("/usr/lib/libretro/fbneo_libretro.so");
        candidates.push_back("/opt/retropie/libretrocores/lr-fbneo/fbneo_libretro.so");
        candidates.push_back("/home/pi/RetroPie/roms/ports/fbneo_libretro.so");
    }

    for (const std::string &path : candidates) {
        if (!path.empty() && regular_file_exists(path)) return path;
    }

    return env && env[0] ? env : (console.corePath ? console.corePath : "");
}

// Downloads a missing core archive and extracts the expected .so file.
bool downloadForConsole(const CoreConfig &console, std::string &error)
{
    if (!console.corePath || !console.corePath[0]) {
        error = "No core path configured for this core.";
        return false;
    }

    std::string core_path = console.corePath;
    std::string core_dir = parent_dir(core_path);
    std::string core_so = file_name_from_path(core_path);
    std::string core_url = console.coreZipUrl ? console.coreZipUrl : "";
    std::string local_archive;

    std::string cmd = "mkdir -p " + shell_quote(core_dir);

    if (is_flycast_env_core(console.envCore)) {
        if (const char *override_so = std::getenv("CP0_DC_CORE_SO_PATH")) {
            if (override_so[0]) {
                std::cout << "Dreamcast core .so override: " << override_so << "\n";

                cmd += " && cp " + shell_quote(override_so) + " " + shell_quote(core_path);

                int rc = std::system(cmd.c_str());
                if (rc != 0) {
                    error = "local .so install failed.";
                    return false;
                }

                return true;
            }
        }

        if (const char *override_path = std::getenv("CP0_DC_CORE_ZIP_PATH")) {
            if (override_path[0]) {
                local_archive = override_path;
                std::cout << "Dreamcast core local archive override: " << local_archive << "\n";
            }
        }

        if (const char *override_url = std::getenv("CP0_DC_CORE_ZIP_URL")) {
            if (override_url[0]) {
                core_url = override_url;
                std::cout << "Dreamcast core download override: " << core_url << "\n";
            }
        }
    }

    if (local_archive.empty() && core_url.empty()) {
        error = "No download URL configured for this core.";
        return false;
    }

    std::string archive = join_path(core_dir, core_so + ".zip");

    if (!local_archive.empty()) {
        cmd += " && cp " + shell_quote(local_archive) + " " + shell_quote(archive);
    } else {
        cmd += " && wget -q -O " + shell_quote(archive) + " " + shell_quote(core_url);
    }

    cmd +=
        " && unzip -o -q " + shell_quote(archive) + " " + shell_quote(core_so) +
        " -d " + shell_quote(core_dir) +
        " && rm -f " + shell_quote(archive);

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        error = "wget/unzip failed.";
        return false;
    }

    if (!regular_file_exists(core_path)) {
        error = "Downloaded file not found: " + core_path;
        return false;
    }

    return true;
}

} // namespace CoreDownloader
