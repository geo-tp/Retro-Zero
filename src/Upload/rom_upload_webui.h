#ifndef CP0_ROM_UPLOAD_WEBUI_H
#define CP0_ROM_UPLOAD_WEBUI_H

#include <string>
#include <vector>

// Builds the ROM upload page HTML. Each entry is {system_key, display_name}.
std::string build_rom_upload_webui_html(
    const std::vector<std::pair<std::string, std::string>> &systems);

#endif // CP0_ROM_UPLOAD_WEBUI_H
