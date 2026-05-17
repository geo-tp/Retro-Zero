
#include "core_registry.h"
#include "../Utils/file_utils.h"

#include <cstring>


// Returns the static list of consoles, tools, paths, and core metadata.
const std::vector<CoreConfig>& CoreRegistry::all()
{
    static const std::vector<CoreConfig> cores = {
        {
            "nes", "Nintendo", "NES", "CP0_CORE_NES",
            "/home/pi/retrozero/cores/quicknes_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/quicknes_libretro.so.zip",
            {".nes", ".fds", ".unf", ".unif"},
            0xFF4C41, false,
            "CP0_ROM_DIR_NES", "/home/pi/roms/nes",
            "nes", false, true
        },
        {
            "snes", "Super Nintendo", "SNES", "CP0_CORE_SNES",
            "/home/pi/retrozero/cores/snes9x_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/snes9x_libretro.so.zip",
            {".sfc", ".smc", ".fig", ".swc", ".bs", ".st"},
            0xDE2400, false,
            "CP0_ROM_DIR_SNES", "/home/pi/roms/snes",
            "snes", false, false
        },
        {
            "gb", "Game Boy", "GB", "CP0_CORE_GB",
            "/home/pi/retrozero/cores/gearboy_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/gearboy_libretro.so.zip",
            {".gb"},
            0xFF999C, false,
            "CP0_ROM_DIR_GB", "/home/pi/roms/gb",
            "gb", false, true
        },
        {
            "gbc", "Game Boy Color", "GBC", "CP0_CORE_GBC",
            "/home/pi/retrozero/cores/gearboy_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/gearboy_libretro.so.zip",
            {".gbc", ".gb"},
            0xFFB4B7, false,
            "CP0_ROM_DIR_GBC", "/home/pi/roms/gbc",
            "gbc", false, true
        },
        {
            "gba", "Game Boy Adv", "GBA", "CP0_CORE_GBA",
            "/home/pi/retrozero/cores/mgba_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/mgba_libretro.so.zip",
            {".gbc", ".gb", ".gba", ".zip"},
            0xFF7F88, false,
            "CP0_ROM_DIR_GBA", "/home/pi/roms/gba",
            "gba", false, false
        },
        {
            "n64", "Nintendo 64", "N64", "CP0_CORE_N64",
            "/home/pi/retrozero/cores/mupen64plus_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/mupen64plus_libretro.so.zip",
            {".n64", ".v64", ".z64", ".bin"},
            0x8F5CFF, false,
            "CP0_ROM_DIR_N64", "/home/pi/roms/n64",
            "n64", true, false
        },
        {
            "md", "Mega Drive", "MD", "CP0_CORE_MD",
            "/home/pi/retrozero/cores/genesis_plus_gx_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/genesis_plus_gx_libretro.so.zip",
            {".md", ".gen", ".bin", ".smd"},
            0x4175FF, false,
            "CP0_ROM_DIR_MD", "/home/pi/roms/md",
            "md", false, false
        },
        {
            "sms", "Master System", "SMS", "CP0_CORE_SMS",
            "/home/pi/retrozero/cores/genesis_plus_gx_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/genesis_plus_gx_libretro.so.zip",
            {".sms"},
            0x39AEFF, false,
            "CP0_ROM_DIR_SMS", "/home/pi/roms/sms",
            "sms", false, true
        },
        {
            "gg", "Game Gear", "GG", "CP0_CORE_GG",
            "/home/pi/retrozero/cores/genesis_plus_gx_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/genesis_plus_gx_libretro.so.zip",
            {".gg"},
            0xC550FF, false,
            "CP0_ROM_DIR_GG", "/home/pi/roms/gg",
            "gg", false, true
        },
        {
            "dc", "Dreamcast", "DC", "CP0_CORE_DC",
            "/home/pi/retrozero/cores/flycast_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/flycast_libretro.so.zip",
            {".cdi", ".gdi", ".chd"},
            0x00AEEF, false,
            "CP0_ROM_DIR_DC", "/home/pi/roms/dc",
            "dc", true, false
        },
        {
            "naomi", "Naomi / Atomis", "NAO", "CP0_CORE_NAOMI_ATOMISWAVE",
            "/home/pi/retrozero/cores/flycast_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/flycast_libretro.so.zip",
            {".zip"},
            0x00D7FF, true,
            "CP0_ROM_DIR_NAOMI", "/home/pi/roms/naomi",
            "naomi", true, false
        },
        {
            "neogeo", "SNK Neo Geo", "NEO", "CP0_CORE_NEOGEO",
            "/home/pi/retrozero/cores/fbneo_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/fbneo_libretro.so.zip",
            {".zip"},
            0x00BE41, true,
            "CP0_ROM_DIR_NEOGEO", "/home/pi/roms/neogeo",
            "neogeo", true, false
        },
        {
            "ngp", "Neo Geo Pocket", "NGP", "CP0_CORE_NGP",
            "/home/pi/retrozero/cores/mednafen_ngp_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/mednafen_ngp_libretro.so.zip",
            {".ngp", ".ngc"},
            0x1FD06C, false,
            "CP0_ROM_DIR_NGP", "/home/pi/roms/ngp",
            "ngp", false, true
        },
        {
            "fbn", "FBNeo Arcade", "FBN", "CP0_CORE_FBN",
            "/home/pi/retrozero/cores/fbneo_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/fbneo_libretro.so.zip",
            {".zip"},
            0x2DDC84, false,
            "CP0_ROM_DIR_FBN", "/home/pi/roms/fbn",
            "fbn", true, false
        },
        {
            "mame", "Mame 2010", "MAME", "CP0_CORE_MAME",
            "/home/pi/retrozero/cores/mame2010_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/mame2010_libretro.so.zip",
            {".zip"},
            0xF25B00, false,
            "CP0_ROM_DIR_MAME", "/home/pi/roms/mame",
            "mame", true, false
        },
        {
            "psp", "Sony PSP", "PSP", "CP0_CORE_PSP",
            "/home/pi/retrozero/cores/ppsspp_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/ppsspp_libretro.so.zip",
            {".iso", ".cso", ".pbp", ".elf", ".prx", ".chd"},
            0x2F78FF, false,
            "CP0_ROM_DIR_PSP", "/home/pi/roms/psp",
            "psp", true, false
        },
        {
            "ps1", "PlayStation", "PS1", "CP0_CORE_PS1",
            "/home/pi/retrozero/cores/pcsx_rearmed_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/pcsx_rearmed_libretro.so.zip",
            {".cue", ".bin", ".chd", ".pbp", ".iso", ".m3u"},
            0xD5D2D5, false,
            "CP0_ROM_DIR_PS1", "/home/pi/roms/ps1",
            "ps1", true, false
        },
        {
            "pce", "PC Engine", "PCE", "CP0_CORE_PCE",
            "/home/pi/retrozero/cores/mednafen_pce_fast_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/mednafen_pce_fast_libretro.so.zip",
            {".pce", ".sgx"},
            0xF61CE6, false,
            "CP0_ROM_DIR_PCE", "/home/pi/roms/pce",
            "pce", false, true
        },
        {
            "msx", "MSX/MSX2", "MSX", "CP0_CORE_MSX",
            "/home/pi/retrozero/cores/bluemsx_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/bluemsx_libretro.so.zip",
            {".rom", ".mx1", ".mx2"},
            0xFFD700, false,
            "CP0_ROM_DIR_MSX", "/home/pi/roms/msx",
            "msx", true, false
        },
        {
            "a2600", "Atari 2600", "2600", "CP0_CORE_A2600",
            "/home/pi/retrozero/cores/stella2014_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/stella2014_libretro.so.zip",
            {".a26", ".bin"},
            0xA35F25, false,
            "CP0_ROM_DIR_A2600", "/home/pi/roms/a2600",
            "a2600", false, true
        },
        {
            "a7800", "Atari 7800", "7800", "CP0_CORE_A7800",
            "/home/pi/retrozero/cores/prosystem_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/prosystem_libretro.so.zip",
            {".a78", ".bin"},
            0xD58A45, false,
            "CP0_ROM_DIR_A7800", "/home/pi/roms/a7800",
            "a7800", false, true
        },
        {
            "lynx", "Atari Lynx", "LYNX", "CP0_CORE_LYNX",
            "/home/pi/retrozero/cores/handy_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/handy_libretro.so.zip",
            {".lnx"},
            0xD5B241, false,
            "CP0_ROM_DIR_LYNX", "/home/pi/roms/lynx",
            "lynx", false, false
        },
        {
            "ws", "WonderSwan", "WS", "CP0_CORE_WS",
            "/home/pi/retrozero/cores/mednafen_wswan_libretro.so",
            "https://raw.githubusercontent.com/geo-tp/Retro-Zero/main/emulators/mednafen_wswan_libretro.so.zip",
            {".ws", ".wsc"},
            0x00FFFF, false,
            "CP0_ROM_DIR_WS", "/home/pi/roms/ws",
            "ws", false, false
        },

        // UI tools — not real emulation cores.
        {
            "", "Settings", "CFG", "CP0_TOOL_SETTINGS",
            "", "",
            {},
            0x3D5A80, false,
            "CP0_ROM_DIR", "/home/pi/roms",
            "", false, false
        },
        {
            "", "ROM Upload", "ROM", "CP0_TOOL_ROM_UPLOAD",
            "", "",
            {},
            0x58C7FF, false,
            "CP0_ROM_DIR", "/home/pi/roms",
            "", false, false
        },
    };

    return cores;
}

// Finds one registry entry by its short internal id.
const CoreConfig* CoreRegistry::findById(const std::string& id)
{
    for (const CoreConfig& c : all()) {
        if (c.id && c.id == id) return &c;
    }
    return nullptr;
}

// Finds one registry entry by the CP0_SELECTED_ENV_CORE value.
const CoreConfig* CoreRegistry::findByEnvCore(const std::string& envCore)
{
    for (const CoreConfig& c : all()) {
        if (c.envCore && envCore == c.envCore) return &c;
    }
    return nullptr;
}

// Finds the first registry entry accepting the ROM file extension.
const CoreConfig* CoreRegistry::findByRomPath(const std::string& romPath)
{
    const std::string ext = lower_ext(romPath);
    if (ext.empty()) return nullptr;

    const CoreConfig* match = nullptr;
    for (const CoreConfig& c : all()) {
        if (!c.id || !c.id[0]) continue; // skip tool entries
        for (const std::string& e : c.extensions) {
            if (e == ext) {
                match = &c;
                break;
            }
        }
    }
    return match;
}


// Decides whether the core should receive only a ROM path instead of in-memory bytes.
bool CoreRegistry::shouldLoadByPathOnly(const std::string& romPath, const char* envCore)
{
    if (envCore && envCore[0]) {
        if (const CoreConfig* cfg = findByEnvCore(envCore)) {
            return cfg->loadByPathOnly;
        }
    }

    // Conservative fallback: if any core with this extension needs path-only,
    // use path-only. Some archive formats are also path-only even when no
    // registry entry is matched.
    const std::string ext = lower_ext(romPath);
    for (const CoreConfig& c : all()) {
        for (const std::string& e : c.extensions) {
            if (e == ext && c.loadByPathOnly) {
                return true;
            }
        }
    }

    return ext == ".7z" || ext == ".gz";
}

// Returns whether the selected system should use the reduced two-button layout.
bool CoreRegistry::isTwoButtonMode(const char* envCore, const std::string& romPath)
{
    if (envCore && envCore[0]) {
        if (const CoreConfig* cfg = findByEnvCore(envCore)) {
            return cfg->twoButtonMode;
        }
    }

    if (const CoreConfig* cfg = findByRomPath(romPath)) {
        return cfg->twoButtonMode;
    }

    return false;
}

// Identifies the dedicated Neo Geo profile for BIOS and audio handling.
bool CoreRegistry::isNeoGeoCore(const char* envCore)
{
    if (envCore && std::strcmp(envCore, "CP0_CORE_NEOGEO") == 0) {
        return true;
    }

    if (const CoreConfig* cfg = findByEnvCore(envCore ? envCore : "")) {
        return cfg->id && std::strcmp(cfg->id, "neogeo") == 0;
    }

    return false;
}

// Identifies the dedicated PSP profile for PPSSPP-specific GLES/options handling.
bool CoreRegistry::isPspCore(const char* envCore)
{
    if (envCore && std::strcmp(envCore, "CP0_CORE_PSP") == 0) {
        return true;
    }

    if (const CoreConfig* cfg = findByEnvCore(envCore ? envCore : "")) {
        return cfg->id && std::strcmp(cfg->id, "psp") == 0;
    }

    return false;
}

// Identifies the dedicated Nintendo 64 profile for HW render/input/load handling.
bool CoreRegistry::isN64Core(const char* envCore)
{
    if (envCore && std::strcmp(envCore, "CP0_CORE_N64") == 0) {
        return true;
    }

    if (const CoreConfig* cfg = findByEnvCore(envCore ? envCore : "")) {
        return cfg->id && std::strcmp(cfg->id, "n64") == 0;
    }

    return false;
}
