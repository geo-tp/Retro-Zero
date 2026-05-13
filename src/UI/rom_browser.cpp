#include "rom_browser.h"

#include "../Utils/file_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string find_rom_dir_for_console(const CoreConfig &console)
{
    if (console.romDirEnv) {
        const char *value = std::getenv(console.romDirEnv);
        if (value && value[0]) return value;
    }

    const char *generic = std::getenv("CP0_ROM_DIR");
    if (generic && generic[0]) return generic;

    return console.romDirDefault ? console.romDirDefault : "/home/pi";
}

bool is_rom_upload_console(const CoreConfig &console)
{
    return console.envCore && std::strcmp(console.envCore, "CP0_TOOL_ROM_UPLOAD") == 0;
}

bool is_settings_console(const CoreConfig &console)
{
    return console.envCore && std::strcmp(console.envCore, "CP0_TOOL_SETTINGS") == 0;
}

namespace {

bool is_supported_rom(const CoreConfig &console, const std::string &name)
{
    const std::string ext = lower_ext(name);
    return std::find(console.extensions.begin(), console.extensions.end(), ext) != console.extensions.end();
}

bool matches_filter(const std::string &name, const std::string &filter)
{
    return filter.empty() ||
           to_lower_copy(name).find(to_lower_copy(filter)) != std::string::npos;
}

} // namespace

// Scans one ROM directory and returns visible entries for the file picker.
std::vector<FileEntry> load_rom_entries(
    const std::string &rom_dir,
    const CoreConfig &console,
    const std::string &filter
)
{
    std::vector<FileEntry> files;
    files.push_back({"..", parent_dir(rom_dir), true});

    DIR *dir = opendir(rom_dir.c_str());
    if (!dir) return files;

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string path = join_path(rom_dir, name);
        struct stat st {};
        if (stat(path.c_str(), &st) != 0) continue;

        const bool is_dir = S_ISDIR(st.st_mode);
        if (is_dir && !matches_filter(name, filter)) continue;
        if (!is_dir && !is_supported_rom(console, name)) continue;
        if (!is_dir && !matches_filter(name, filter)) continue;

        files.push_back({name, path, is_dir});
    }
    closedir(dir);

    if (files.size() > 1) {
        std::sort(files.begin() + 1, files.end(), [](const FileEntry &a, const FileEntry &b) {
            if (a.dir != b.dir) return a.dir > b.dir;
            return to_lower_copy(a.name) < to_lower_copy(b.name);
        });
    }

    return files;
}
