#ifndef CP0_RETRO_SAVE_H
#define CP0_RETRO_SAVE_H

#include <cstdint>
#include <string>
#include <vector>

class LibretroCore;

// Computes the default save directory for a selected emulation profile.
std::string default_save_dir_for_core(const char *env_core, const char *rom_path);

// Mirrors Libretro SRAM/RTC memory regions to disk with periodic dirty checks.
class RetroSave {
public:
    // Binds the save synchronizer to one core and one ROM save namespace.
    void init(LibretroCore *core, const std::string &save_dir, const std::string &rom_path);

    // Loads existing SRAM/RTC files into the core memory regions when available.
    void load_from_disk_if_available();

    // Detects modified save regions and flushes them after a debounce delay.
    void tick();

    // Writes all dirty save regions immediately.
    void flush_if_dirty();

private:
    struct Region {
        unsigned id;
        std::string path;
        std::vector<uint8_t> snapshot;
        bool seen = false;
        bool dirty = false;
        uint64_t last_write_ms = 0;
    };

    bool read_region(unsigned id, std::vector<uint8_t> &out);
    bool write_region(unsigned id, const std::vector<uint8_t> &in, size_t &written);

    LibretroCore *core_ = nullptr;
    std::vector<Region> regions_;
    uint64_t last_check_ms_ = 0;
};

#endif // CP0_RETRO_SAVE_H
