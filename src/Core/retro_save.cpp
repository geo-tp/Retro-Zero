#include "retro_save.h"

#include "libretro_core.h"
#include "core_registry.h"
#include "../Utils/file_utils.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {

bool read_file_bytes(const std::string &path, std::vector<uint8_t> &out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        out.clear();
        return false;
    }

    std::streamsize size = file.tellg();
    if (size < 0) {
        out.clear();
        return false;
    }
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char *>(out.data()), size)) {
        out.clear();
        return false;
    }
    return true;
}

bool write_file_atomic(const std::string &path, const std::vector<uint8_t> &data)
{
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "save: failed to open " << tmp_path << "\n";
            return false;
        }
        if (!data.empty()) {
            out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!out.good()) {
                std::cerr << "save: failed to write " << tmp_path << "\n";
                return false;
            }
        }
    }

    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::cerr << "save: rename failed " << tmp_path << " -> " << path
                  << ": " << std::strerror(errno) << "\n";
        std::remove(tmp_path.c_str());
        return false;
    }
    return true;
}

uint64_t monotonic_ms()
{
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count());
}

std::string file_stem(const std::string &path)
{
    if (path.empty()) return "game";
    size_t slash = path.find_last_of('/');
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (name.empty()) return "game";
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return name;
    return name.substr(0, dot);
}

} // namespace

// Maps a selected core profile to its default save directory.
std::string default_save_dir_for_core(const char *env_core, const char *rom_path)
{
    const char *subdir = nullptr;

    if (env_core && env_core[0]) {
        if (const CoreConfig *cfg = CoreRegistry::findByEnvCore(env_core)) {
            subdir = cfg->saveSubdir;
        }
    }

    if ((!subdir || !subdir[0]) && rom_path && rom_path[0]) {
        if (const CoreConfig *cfg = CoreRegistry::findByRomPath(rom_path)) {
            subdir = cfg->saveSubdir;
        }
    }

    if (!subdir || !subdir[0]) {
        subdir = "misc";
    }

    return std::string("/home/pi/retrozero/saves/") + subdir;
}

// Registers SRAM/RTC memory regions that should be mirrored to disk.
void RetroSave::init(LibretroCore *core, const std::string &save_dir, const std::string &rom_path)
{
    core_ = core;
    last_check_ms_ = monotonic_ms();

    const std::string stem = file_stem(rom_path);
    regions_.clear();
    regions_.push_back(Region{RETRO_MEMORY_SAVE_RAM, join_path(save_dir, stem + ".srm"), {}, false, false, 0});
    regions_.push_back(Region{RETRO_MEMORY_RTC, join_path(save_dir, stem + ".rtc"), {}, false, false, 0});
}

// Restores existing save files into the core memory regions.
void RetroSave::load_from_disk_if_available()
{
    for (auto &region : regions_) {
        std::vector<uint8_t> disk;
        if (read_file_bytes(region.path, disk) && !disk.empty()) {
            size_t loaded = 0;
            if (write_region(region.id, disk, loaded)) {
                std::vector<uint8_t> mem;
                if (read_region(region.id, mem)) {
                    region.snapshot = std::move(mem);
                    region.seen = true;
                }
                std::cout << "save: loaded " << region.path << " (" << loaded << " bytes)\n";
            }
        }
    }
}

// Periodically checks for save changes and flushes debounced writes.
void RetroSave::tick()
{
    const uint64_t now = monotonic_ms();
    if (now - last_check_ms_ < 2000) return;
    last_check_ms_ = now;

    for (auto &region : regions_) {
        std::vector<uint8_t> cur;
        if (!read_region(region.id, cur)) continue;

        if (!region.seen) {
            region.snapshot = cur;
            region.seen = true;
            std::vector<uint8_t> disk;
            if (!read_file_bytes(region.path, disk) || disk != region.snapshot) {
                region.dirty = true;
            }
            continue;
        }

        if (cur != region.snapshot) {
            region.snapshot.swap(cur);
            region.dirty = true;
        }

        if (region.dirty && (now - region.last_write_ms >= 15000)) {
            if (write_file_atomic(region.path, region.snapshot)) {
                region.dirty = false;
                region.last_write_ms = now;
            }
        }
    }
}

// Writes all dirty save regions immediately.
void RetroSave::flush_if_dirty()
{
    const uint64_t now = monotonic_ms();
    for (auto &region : regions_) {
        std::vector<uint8_t> cur;
        if (!read_region(region.id, cur)) continue;

        if (!region.seen) {
            region.snapshot = cur;
            region.seen = true;
            std::vector<uint8_t> disk;
            if (!read_file_bytes(region.path, disk) || disk != region.snapshot) {
                region.dirty = true;
            }
        } else if (cur != region.snapshot) {
            region.snapshot.swap(cur);
            region.dirty = true;
        }

        if (region.dirty && write_file_atomic(region.path, region.snapshot)) {
            region.dirty = false;
            region.last_write_ms = now;
        }
    }
}

bool RetroSave::read_region(unsigned id, std::vector<uint8_t> &out)
{
    if (!core_ || !core_->retro_get_memory_data || !core_->retro_get_memory_size) return false;
    void *ptr = core_->retro_get_memory_data(id);
    const size_t size = core_->retro_get_memory_size(id);
    if (!ptr || size == 0) return false;
    const uint8_t *p = static_cast<const uint8_t *>(ptr);
    out.assign(p, p + size);
    return true;
}

bool RetroSave::write_region(unsigned id, const std::vector<uint8_t> &in, size_t &written)
{
    written = 0;
    if (!core_ || !core_->retro_get_memory_data || !core_->retro_get_memory_size) return false;
    void *ptr = core_->retro_get_memory_data(id);
    const size_t size = core_->retro_get_memory_size(id);
    if (!ptr || size == 0) return false;
    written = std::min(size, in.size());
    if (written == 0) return false;
    std::memcpy(ptr, in.data(), written);
    return true;
}
