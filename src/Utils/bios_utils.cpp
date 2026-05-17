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

constexpr const char *kNaomiRomDir = "/home/pi/roms/naomi";
constexpr const char *kNaomi2RomDir = "/home/pi/roms/naomi2";
constexpr const char *kAtomiswaveRomDir = "/home/pi/roms/atomiswave";

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

bool string_contains_case_insensitive(const std::string &value, const std::string &needle)
{
    return lower_string(value).find(lower_string(needle)) != std::string::npos;
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

void log_file_state(const char *prefix, const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        std::cout << prefix << ": " << path << " MISSING\n";
        return;
    }

    std::cout << prefix << ": " << path
              << " present size=" << static_cast<long long>(st.st_size)
              << " md5=" << md5sum_file(path) << "\n";
}

bool files_are_same_content(const std::string &a, const std::string &b)
{
    struct stat sa {};
    struct stat sb {};
    if (stat(a.c_str(), &sa) != 0 || stat(b.c_str(), &sb) != 0) {
        return false;
    }
    if (!S_ISREG(sa.st_mode) || !S_ISREG(sb.st_mode)) {
        return false;
    }
    if (sa.st_size != sb.st_size) {
        return false;
    }

    return md5sum_file(a) == md5sum_file(b);
}

bool copy_bios_if_present_and_different(
    const std::string &src_path,
    const std::string &dst_path,
    const char *label
)
{
    if (!regular_file_exists(src_path)) {
        return false;
    }

    if (src_path == dst_path || (regular_file_exists(dst_path) && files_are_same_content(src_path, dst_path))) {
        std::cout << label << ": " << dst_path << " already matches " << src_path << "\n";
        return true;
    }

    if (!copy_file_overwrite_if_present(src_path, dst_path)) {
        std::cout << label << ": failed to copy " << src_path
                  << " -> " << dst_path << "\n";
        return false;
    }

    std::cout << label << ": updated " << src_path
              << " -> " << dst_path << "\n";

    return regular_file_exists(dst_path);
}

bool flycast_env_core_matches(const char *env_core)
{
    if (!env_core) return false;

    return std::strcmp(env_core, "CP0_CORE_DC") == 0
        || std::strcmp(env_core, "CP0_CORE_DREAMCAST") == 0
        || std::strcmp(env_core, "CP0_CORE_NAOMI") == 0
        || std::strcmp(env_core, "CP0_CORE_NAOMI2") == 0
        || std::strcmp(env_core, "CP0_CORE_ATOMISWAVE") == 0
        || std::strcmp(env_core, "CP0_CORE_NAOMI_ATOMISWAVE") == 0;
}

bool is_dreamcast_core(const char *env_core)
{
    if (!env_core) return false;

    return std::strcmp(env_core, "CP0_CORE_DC") == 0
        || std::strcmp(env_core, "CP0_CORE_DREAMCAST") == 0;
}

bool is_naomi_core(const char *env_core)
{
    if (!env_core) return false;

    return std::strcmp(env_core, "CP0_CORE_NAOMI") == 0
        || std::strcmp(env_core, "CP0_CORE_NAOMI2") == 0
        || std::strcmp(env_core, "CP0_CORE_NAOMI_ATOMISWAVE") == 0;
}

bool is_naomi2_core(const char *env_core)
{
    return env_core && std::strcmp(env_core, "CP0_CORE_NAOMI2") == 0;
}

bool is_atomiswave_core(const char *env_core)
{
    return env_core &&
        (std::strcmp(env_core, "CP0_CORE_ATOMISWAVE") == 0 ||
         std::strcmp(env_core, "CP0_CORE_NAOMI_ATOMISWAVE") == 0);
}

bool is_psp_core(const char *env_core)
{
    return env_core && std::strcmp(env_core, "CP0_CORE_PSP") == 0;
}

bool content_dir_looks_like_naomi(const std::string &content_dir)
{
    return string_contains_case_insensitive(content_dir, "/naomi")
        || string_contains_case_insensitive(content_dir, "\\naomi");
}

bool content_dir_looks_like_naomi2(const std::string &content_dir)
{
    return string_contains_case_insensitive(content_dir, "/naomi2")
        || string_contains_case_insensitive(content_dir, "\\naomi2");
}

bool content_dir_looks_like_atomiswave(const std::string &content_dir)
{
    return string_contains_case_insensitive(content_dir, "/atomiswave")
        || string_contains_case_insensitive(content_dir, "\\atomiswave");
}

void add_flycast_bios_search_dirs(
    std::vector<std::string> &dirs,
    const std::string &content_dir
)
{
    if (!content_dir.empty()) {
        add_unique_dir(dirs, content_dir);
        add_unique_dir(dirs, parent_dir(content_dir));
    }

    add_unique_dir(dirs, kNaomiRomDir);
    add_unique_dir(dirs, kNaomi2RomDir);
    add_unique_dir(dirs, kAtomiswaveRomDir);

    add_unique_dir(dirs, std::string(kRetroZeroSystemRoot) + "/dc");
    add_unique_dir(dirs, kRetroZeroSystemRoot);
}

void log_flycast_arcade_bios_state(const char *label)
{
    const std::string dc_system_dir = std::string(kRetroZeroSystemRoot) + "/dc";

    std::cout << "Flycast arcade BIOS check: " << (label ? label : "") << "\n";

    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/naomi.zip");
    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/naomi2.zip");
    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/awbios.zip");

    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/hod2bios.zip");
    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/f355dlx.zip");
    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/f355bios.zip");
    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/airlbios.zip");
    log_file_state("Flycast arcade BIOS check", dc_system_dir + "/segasp.zip");

    log_file_state("Flycast arcade BIOS check", std::string(kNaomiRomDir) + "/naomi.zip");
    log_file_state("Flycast arcade BIOS check", std::string(kNaomi2RomDir) + "/naomi2.zip");
    log_file_state("Flycast arcade BIOS check", std::string(kAtomiswaveRomDir) + "/awbios.zip");
}

bool install_flycast_bios_zip_if_present(
    const std::string &content_dir,
    const char *bios_zip_name,
    const std::string &dc_system_dir
)
{
    if (!bios_zip_name || !bios_zip_name[0]) return false;

    const std::string dst_path = join_path(dc_system_dir, bios_zip_name);

    std::vector<std::string> search_dirs;
    add_flycast_bios_search_dirs(search_dirs, content_dir);

    std::string src_path;
    if (!find_file_case_insensitive(search_dirs, bios_zip_name, src_path)) {
        std::cout << "Flycast BIOS: " << bios_zip_name << " MISSING, searched:\n";
        for (const std::string &dir : search_dirs) {
            std::cout << "  " << dir << "\n";
        }
        return false;
    }

    ensure_dir_exists(parent_dir(dst_path));

    return copy_bios_if_present_and_different(src_path, dst_path, "Flycast BIOS");
}

struct FlycastBiosState {
    bool dc_boot_ok = false;
    bool dc_flash_ok = false;

    bool naomi_ok = false;
    bool naomi2_ok = false;
    bool atomiswave_ok = false;

    bool hod2_ok = false;
    bool f355dlx_ok = false;
    bool f355_ok = false;
    bool airlbios_ok = false;
    bool segasp_ok = false;
};

FlycastBiosState prepare_flycast_bios(const std::string &content_dir)
{
    FlycastBiosState state;

    const std::string dc_system_dir = std::string(kRetroZeroSystemRoot) + "/dc";
    if (!ensure_dir_exists(dc_system_dir)) {
        std::cout << "Flycast BIOS: failed to create " << dc_system_dir << "\n";
        return state;
    }

    log_flycast_arcade_bios_state("before install");

    state.naomi_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "naomi.zip",
        dc_system_dir
    );

    state.naomi2_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "naomi2.zip",
        dc_system_dir
    );

    state.atomiswave_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "awbios.zip",
        dc_system_dir
    );

    state.hod2_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "hod2bios.zip",
        dc_system_dir
    );

    state.f355dlx_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "f355dlx.zip",
        dc_system_dir
    );

    state.f355_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "f355bios.zip",
        dc_system_dir
    );

    state.airlbios_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "airlbios.zip",
        dc_system_dir
    );

    state.segasp_ok = install_flycast_bios_zip_if_present(
        content_dir,
        "segasp.zip",
        dc_system_dir
    );

    state.naomi_ok = regular_file_exists(dc_system_dir + "/naomi.zip");
    state.naomi2_ok = regular_file_exists(dc_system_dir + "/naomi2.zip");
    state.atomiswave_ok = regular_file_exists(dc_system_dir + "/awbios.zip");

    state.hod2_ok = regular_file_exists(dc_system_dir + "/hod2bios.zip");
    state.f355dlx_ok = regular_file_exists(dc_system_dir + "/f355dlx.zip");
    state.f355_ok = regular_file_exists(dc_system_dir + "/f355bios.zip");
    state.airlbios_ok = regular_file_exists(dc_system_dir + "/airlbios.zip");
    state.segasp_ok = regular_file_exists(dc_system_dir + "/segasp.zip");

    log_flycast_arcade_bios_state("after install");

    std::cout << "Flycast arcade BIOS summary:\n";
    std::cout << "  naomi.zip: " << (state.naomi_ok ? "present" : "MISSING") << "\n";
    std::cout << "  naomi2.zip: " << (state.naomi2_ok ? "present" : "MISSING") << "\n";
    std::cout << "  awbios.zip: " << (state.atomiswave_ok ? "present" : "MISSING") << "\n";
    std::cout << "  hod2bios.zip: " << (state.hod2_ok ? "present" : "MISSING/optional") << "\n";
    std::cout << "  f355dlx.zip: " << (state.f355dlx_ok ? "present" : "MISSING/optional") << "\n";
    std::cout << "  f355bios.zip: " << (state.f355_ok ? "present" : "MISSING/optional") << "\n";
    std::cout << "  airlbios.zip: " << (state.airlbios_ok ? "present" : "MISSING/optional") << "\n";
    std::cout << "  segasp.zip: " << (state.segasp_ok ? "present" : "MISSING/optional") << "\n";

    return state;
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

bool is_flycast_env_core(const char *env_core)
{
    return flycast_env_core_matches(env_core);
}

// Logs BIOS files relevant to the selected core for debugging startup issues.
void log_runtime_bios_state_for_core(const char *env_core, const char *label)
{
    if (is_naomi_core(env_core) || is_atomiswave_core(env_core)) {
        log_flycast_arcade_bios_state(label);
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

    if (is_flycast_env_core(env_core)) {
        system_dir = kRetroZeroSystemRoot;
        std::cout << "Flycast system dir forced: " << system_dir << "\n";

        if (is_dreamcast_core(env_core)) {
            return true;
        }

        FlycastBiosState flycast_bios = prepare_flycast_bios(content_dir);

        const bool content_seems_naomi = content_dir_looks_like_naomi(content_dir);
        const bool content_seems_naomi2 = content_dir_looks_like_naomi2(content_dir);
        const bool content_seems_atomiswave = content_dir_looks_like_atomiswave(content_dir);

        if ((is_naomi2_core(env_core) || content_seems_naomi2) && !flycast_bios.naomi2_ok) {
            error_message =
                "Naomi 2 BIOS missing: expected naomi2.zip in /home/pi/roms/naomi2, "
                "the ROM folder, or " + system_dir + "/dc";
            return false;
        }

        if ((is_naomi_core(env_core) || content_seems_naomi) && !flycast_bios.naomi_ok) {
            error_message =
                "Naomi BIOS missing: expected naomi.zip in /home/pi/roms/naomi, "
                "the ROM folder, or " + system_dir + "/dc";
            return false;
        }

        if ((is_atomiswave_core(env_core) || content_seems_atomiswave) && !flycast_bios.atomiswave_ok) {
            error_message =
                "Atomiswave BIOS missing: expected awbios.zip in /home/pi/roms/atomiswave, "
                "the ROM folder, or " + system_dir + "/dc";
            return false;
        }

        return true;
    }

    if (is_psp_core(env_core)) {
        system_dir = kRetroZeroSystemRoot;
        std::cout << "PPSSPP system dir forced: " << system_dir
                  << " (place PPSSPP assets under " << system_dir
                  << "/PPSSPP if your core build requires them)\n";
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

    result.ok = find_file_case_insensitive(
        result.searched_dirs,
        "neogeo.zip",
        result.neogeo_zip_path
    );

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
