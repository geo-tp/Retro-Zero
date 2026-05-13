#ifndef CP0_ROM_UPLOAD_SERVER_H
#define CP0_ROM_UPLOAD_SERVER_H

#include "../Core/core_config.h"

#include <string>
#include <vector>

namespace UploadServer {

struct StartResult {
    bool ok = false;
    std::string error;
    std::string message;
    int port = 0;
};

// Starts the lightweight HTTP upload server used by the ROM Upload tool.
StartResult start(const std::vector<CoreConfig> &consoles);

// Reports whether the upload server thread is already active.
bool running();

} // namespace UploadServer

#endif // CP0_ROM_UPLOAD_SERVER_H
