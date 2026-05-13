#include "bios_utils.h"

#include "file_utils.h"
#include "../Assets/embedded_cbios.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <dirent.h>

namespace {

constexpr const char *kRetroZeroSystemRoot = "/home/pi/retrozero/system";
constexpr const char *kMsxSystemDir = "/home/pi/retrozero/system/bluemsx";
constexpr const char *kDreamcastRomDir = "/home/pi/roms/dc";

std::string lower_string(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

void add_unique_dir(std::vector<std::string> &dirs, const std::string &dir)
{
    if (dir.empty()) return;
    if (std::find(dirs.begin(), dirs.end(), dir) == dirs.end()) {
        dirs.push_back(dir);
    }
}

bool find_file_case_insensitive(
    const std::vector<std::string> &dirs,
    const char *file_name,
    std::string &out_path
)
{
    const std::string target = lower_string(file_name ? file_name : "");
    for (const std::string &dir : dirs) {
        if (dir.empty()) continue;

        const std::string exact_path = join_path(dir, file_name ? file_name : "");
        if (regular_file_exists(exact_path)) {
            out_path = exact_path;
            return true;
        }

        DIR *dp = opendir(dir.c_str());
        if (!dp) continue;

        struct dirent *entry = nullptr;
        while ((entry = readdir(dp)) != nullptr) {
            const char *name = entry->d_name;
            if (!name || name[0] == '\0') continue;
            if (lower_string(name) != target) continue;

            const std::string candidate = join_path(dir, name);
            if (regular_file_exists(candidate)) {
                closedir(dp);
                out_path = candidate;
                return true;
            }
        }
        closedir(dp);
    }

    out_path.clear();
    return false;
}

void log_bios_file(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        std::cout << "Dreamcast BIOS check: " << path << " MISSING\n";
        return;
    }
    std::cout << "Dreamcast BIOS check: " << path
              << " present size=" << static_cast<long long>(st.st_size)
              << " md5=" << md5sum_file(path) << "\n";
}

void log_dreamcast_bios_state(const char *label)
{
    std::cout << "Dreamcast BIOS check: " << (label ? label : "") << "\n";
    log_bios_file(std::string(kDreamcastRomDir) + "/dc_boot.bin");
    log_bios_file(std::string(kDreamcastRomDir) + "/dc_flash.bin");
    log_bios_file(std::string(kRetroZeroSystemRoot) + "/dc/dc_boot.bin");
    log_bios_file(std::string(kRetroZeroSystemRoot) + "/dc/dc_flash.bin");
}

bool prepare_dreamcast_bios(const std::string &content_dir)
{
    const std::string dc_system_dir = std::string(kRetroZeroSystemRoot) + "/dc";
    if (!ensure_dir_exists(dc_system_dir)) return false;

    const std::string boot_dst = dc_system_dir + "/dc_boot.bin";
    const std::string flash_dst = dc_system_dir + "/dc_flash.bin";

    log_dreamcast_bios_state("before install");
    copy_file_overwrite_if_present(std::string(kDreamcastRomDir) + "/dc_boot.bin", boot_dst);
    copy_file_overwrite_if_present(std::string(kDreamcastRomDir) + "/dc_flash.bin", flash_dst);
    copy_file_if_missing(content_dir + "/dc_boot.bin", boot_dst);
    copy_file_if_missing(content_dir + "/dc_flash.bin", flash_dst);
    log_dreamcast_bios_state("after install");

    const bool boot_ok = regular_file_exists(boot_dst);
    const bool flash_ok = regular_file_exists(flash_dst);
    std::cout << "Dreamcast BIOS: " << boot_dst << " "
              << (boot_ok ? "present" : "MISSING") << "\n";
    std::cout << "Dreamcast BIOS: " << flash_dst << " "
              << (flash_ok ? "present" : "MISSING") << "\n";
    return boot_ok && flash_ok;
}

bool install_embedded_cbios(const std::string &system_dir)
{
    if (!ensure_dir_exists(system_dir)) return false;

    size_t count = 0;
    const EmbeddedCbiosFile *files = embedded_cbios_files(&count);
    for (size_t i = 0; i < count; ++i) {
        const std::string dst = join_path(system_dir, files[i].name);
        ensure_dir_exists(parent_dir(dst));
        if (!write_bytes_if_missing(dst, files[i].data, files[i].size)) {
            return false;
        }
    }
    ensure_dir_exists(join_path(system_dir, "Databases"));

    std::cout << "C-BIOS: ready in " << system_dir << "\n";
    return true;
}

} // namespace

// Logs BIOS files relevant to the selected core for debugging startup issues.
void log_runtime_bios_state_for_core(const char *env_core, const char *label)
{
    if (env_core && std::strcmp(env_core, "CP0_CORE_DC") == 0) {
        log_dreamcast_bios_state(label);
    }
}

// Creates or locates BIOS files needed before retro_load_game.
bool prepare_runtime_bios_for_core(
    const char *env_core,
    const std::string &content_dir,
    bool system_dir_overridden,
    std::string &system_dir,
    std::string &error_message
)
{
    error_message.clear();

    if (!env_core || !env_core[0]) {
        return true;
    }

    if (std::strcmp(env_core, "CP0_CORE_MSX") == 0) {
        if (!system_dir_overridden) {
            system_dir = kMsxSystemDir;
        }
        if (!install_embedded_cbios(system_dir)) {
            error_message = "MSX C-BIOS: failed to install embedded C-BIOS files in " + system_dir;
            return false;
        }
        return true;
    }

    if (std::strcmp(env_core, "CP0_CORE_DC") == 0) {
        system_dir = kRetroZeroSystemRoot;
        std::cout << "Dreamcast system dir forced: " << system_dir << "\n";
        if (!prepare_dreamcast_bios(content_dir)) {
            error_message = "Dreamcast BIOS: original BIOS required; expected dc_boot.bin and dc_flash.bin in /home/pi/roms/dc or " + system_dir + "/dc";
        }
        return true;
    }

    return true;
}

// Validates that neogeo.zip is available next to arcade ROMs or configured paths.
NeoGeoBiosCheck check_neogeo_bios_for_rom(
    const std::string &rom_path,
    const std::string &current_rom_dir,
    const std::string &configured_rom_dir
)
{
    NeoGeoBiosCheck result;

    const std::string rom_folder = parent_dir(rom_path);
    add_unique_dir(result.searched_dirs, rom_folder);
    add_unique_dir(result.searched_dirs, current_rom_dir);
    add_unique_dir(result.searched_dirs, configured_rom_dir);
    add_unique_dir(result.searched_dirs, parent_dir(rom_folder));

    result.ok = find_file_case_insensitive(result.searched_dirs, "neogeo.zip", result.neogeo_zip_path);
    if (result.ok) {
        result.bios_dir = parent_dir(result.neogeo_zip_path);
        return result;
    }

    result.error_body = "neogeo.zip missing\n\nsearched:\n";
    for (const std::string &dir : result.searched_dirs) {
        result.error_body += dir + "\n";
    }
    return result;
}
