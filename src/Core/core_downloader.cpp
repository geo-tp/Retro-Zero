#include "core_downloader.h"
#include "core_registry.h"
#include "../Utils/bios_utils.h"
#include "../Utils/file_utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char *kPspAssetsArchiveName = "ppsspp_assets.zip";
constexpr const char *kPspAssetsDir = "/home/pi/retrozero/system/PPSSPP";
constexpr const char *kPspAssetsUrl =
    "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/ppsspp_assets.zip";

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

bool command_ok(const std::string &cmd)
{
    return std::system(cmd.c_str()) == 0;
}

bool zip_contains_file(const std::string &archive, const std::string &name)
{
    const std::string cmd =
        "unzip -Z1 " + shell_quote(archive) +
        " | grep -qx " + shell_quote(name);
    return command_ok(cmd);
}

bool zip_contains_ppsspp_tree(const std::string &archive)
{
    const std::string cmd =
        "unzip -Z1 " + shell_quote(archive) +
        " | grep -q '^PPSSPP/'";
    return command_ok(cmd);
}

bool install_psp_assets_from_archive(
    const std::string &archive,
    const std::string &core_dir,
    std::string &error
)
{
    if (archive.empty() || !regular_file_exists(archive)) {
        error = "PPSSPP assets archive not found.";
        return false;
    }

    if (zip_contains_ppsspp_tree(archive)) {
        const std::string cmd =
            "mkdir -p " + shell_quote(parent_dir(kPspAssetsDir)) +
            " && unzip -o -q " + shell_quote(archive) +
            " 'PPSSPP/*' -d " + shell_quote(parent_dir(kPspAssetsDir));
        if (!command_ok(cmd)) {
            error = "PPSSPP assets extract failed.";
            return false;
        }
        std::cout << "PPSSPP assets installed from core archive PPSSPP/ tree\n";
        return true;
    }

    if (zip_contains_file(archive, kPspAssetsArchiveName)) {
        const std::string nested_archive = join_path(core_dir, kPspAssetsArchiveName);
        const std::string cmd =
            "unzip -o -q " + shell_quote(archive) + " " +
            shell_quote(kPspAssetsArchiveName) + " -d " + shell_quote(core_dir) +
            " && mkdir -p " + shell_quote(kPspAssetsDir) +
            " && unzip -o -q " + shell_quote(nested_archive) +
            " -d " + shell_quote(kPspAssetsDir) +
            " && rm -f " + shell_quote(nested_archive);
        if (!command_ok(cmd)) {
            error = "PPSSPP nested assets extract failed.";
            return false;
        }
        std::cout << "PPSSPP assets installed from nested " << kPspAssetsArchiveName << "\n";
        return true;
    }

    error = "PPSSPP assets not present in core archive.";
    return false;
}

bool install_psp_assets_fallback(const std::string &core_dir, std::string &error)
{
    std::string assets_archive;
    if (const char *override_path = std::getenv("CP0_PSP_ASSETS_ZIP_PATH")) {
        if (override_path[0]) {
            assets_archive = override_path;
            std::cout << "PPSSPP assets local archive override: " << assets_archive << "\n";
        }
    }

    const std::string tmp_archive = join_path(core_dir, kPspAssetsArchiveName);
    std::string cmd = "mkdir -p " + shell_quote(core_dir);
    bool remove_tmp = false;

    if (!assets_archive.empty()) {
        cmd += " && cp " + shell_quote(assets_archive) + " " + shell_quote(tmp_archive);
        remove_tmp = true;
    } else {
        const char *assets_url = kPspAssetsUrl;
        if (const char *override_url = std::getenv("CP0_PSP_ASSETS_ZIP_URL")) {
            if (override_url[0]) {
                assets_url = override_url;
                std::cout << "PPSSPP assets download override: " << assets_url << "\n";
            }
        }
        cmd += " && wget -q -O " + shell_quote(tmp_archive) + " " + shell_quote(assets_url);
        remove_tmp = true;
    }

    cmd +=
        " && mkdir -p " + shell_quote(kPspAssetsDir) +
        " && unzip -o -q " + shell_quote(tmp_archive) +
        " -d " + shell_quote(kPspAssetsDir);
    if (remove_tmp) {
        cmd += " && rm -f " + shell_quote(tmp_archive);
    }

    if (!command_ok(cmd)) {
        error = "PPSSPP assets download/extract failed.";
        return false;
    }

    std::cout << "PPSSPP assets installed under " << kPspAssetsDir << "\n";
    return true;
}

bool install_psp_assets(
    const std::string &core_archive,
    const std::string &core_dir,
    std::string &error
)
{
    std::string archive_error;
    if (install_psp_assets_from_archive(core_archive, core_dir, archive_error)) {
        return true;
    }

    std::cout << "PPSSPP assets: " << archive_error
              << " Trying fallback archive.\n";
    return install_psp_assets_fallback(core_dir, error);
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

                if (!command_ok(cmd)) {
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

    if (CoreRegistry::isPspCore(console.envCore)) {
        if (const char *override_so = std::getenv("CP0_PSP_CORE_SO_PATH")) {
            if (override_so[0]) {
                std::cout << "PPSSPP core .so override: " << override_so << "\n";

                cmd += " && cp " + shell_quote(override_so) + " " + shell_quote(core_path);

                if (!command_ok(cmd)) {
                    error = "local .so install failed.";
                    return false;
                }

                if (!install_psp_assets_fallback(core_dir, error)) {
                    return false;
                }

                return true;
            }
        }

        if (const char *override_path = std::getenv("CP0_PSP_CORE_ZIP_PATH")) {
            if (override_path[0]) {
                local_archive = override_path;
                std::cout << "PPSSPP core local archive override: " << local_archive << "\n";
            }
        }

        if (const char *override_url = std::getenv("CP0_PSP_CORE_ZIP_URL")) {
            if (override_url[0]) {
                core_url = override_url;
                std::cout << "PPSSPP core download override: " << core_url << "\n";
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
        " -d " + shell_quote(core_dir);

    if (!command_ok(cmd)) {
        error = "wget/unzip failed.";
        return false;
    }

    if (!regular_file_exists(core_path)) {
        error = "Downloaded file not found: " + core_path;
        return false;
    }

    if (CoreRegistry::isPspCore(console.envCore) &&
        !install_psp_assets(archive, core_dir, error)) {
        return false;
    }

    std::remove(archive.c_str());

    return true;
}

} // namespace CoreDownloader
