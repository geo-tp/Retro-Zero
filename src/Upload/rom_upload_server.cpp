#include "rom_upload_server.h"
#include "rom_upload_webui.h"
#include "../Utils/file_utils.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <iostream>
#include <map>
#include <net/if.h>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>

namespace {

std::atomic<bool> g_server_running {false};
int g_server_fd = -1;
int g_server_port = 80;
std::vector<CoreConfig> g_consoles;

constexpr int kPreferredPort = 80;
constexpr int kFallbackPort = 8080;

std::string lower_string(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string trim_copy(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

bool is_rom_upload_console(const CoreConfig &console)
{
    return console.envCore && std::strcmp(console.envCore, "CP0_TOOL_ROM_UPLOAD") == 0;
}

bool is_settings_console(const CoreConfig &console)
{
    return console.envCore && std::strcmp(console.envCore, "CP0_TOOL_SETTINGS") == 0;
}

bool get_wifi_ipv4(std::string &out_ip)
{
    out_ip.clear();
    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0 || !ifaddr) {
        return false;
    }

    for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!ifa->ifa_name || std::strncmp(ifa->ifa_name, "wl", 2) != 0) continue;
        if ((ifa->ifa_flags & IFF_UP) == 0) continue;

        auto *sa = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
        char ip[INET_ADDRSTRLEN] {};
        if (!inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) continue;
        std::string value = ip;
        if (value.rfind("169.254.", 0) == 0) continue;
        if (value.rfind("127.", 0) == 0) continue;
        out_ip = value;
        break;
    }

    freeifaddrs(ifaddr);
    return !out_ip.empty();
}

bool read_http_line(int fd, std::string &line)
{
    line.clear();
    char ch = 0;
    while (true) {
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n <= 0) return false;
        if (ch == '\r') continue;
        if (ch == '\n') return true;
        line.push_back(ch);
        if (line.size() > 8192) return false;
    }
}

void send_http_response(int fd, const char *status, const char *content_type, const std::string &body)
{
    std::string head = std::string("HTTP/1.1 ") + status + "\r\n" +
                       "Content-Type: " + content_type + "\r\n" +
                       "Content-Length: " + std::to_string(body.size()) + "\r\n" +
                       "Connection: close\r\n\r\n";
    send(fd, head.data(), head.size(), 0);
    if (!body.empty()) send(fd, body.data(), body.size(), 0);
}

std::string url_decode(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char h1 = s[i + 1];
            char h2 = s[i + 2];
            auto hexv = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                return -1;
            };
            int a = hexv(h1);
            int b = hexv(h2);
            if (a >= 0 && b >= 0) {
                out.push_back(static_cast<char>((a << 4) | b));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i] == '+' ? ' ' : s[i]);
    }
    return out;
}

std::map<std::string, std::string> parse_query(const std::string &query)
{
    std::map<std::string, std::string> kv;
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        if (amp == std::string::npos) amp = query.size();
        std::string token = query.substr(pos, amp - pos);
        size_t eq = token.find('=');
        std::string k = eq == std::string::npos ? token : token.substr(0, eq);
        std::string v = eq == std::string::npos ? "" : token.substr(eq + 1);
        kv[url_decode(k)] = url_decode(v);
        pos = amp + 1;
    }
    return kv;
}

std::string sanitize_filename(const std::string &name)
{
    std::string base = file_name_from_path(name);
    std::string out;
    out.reserve(base.size());
    for (char c : base) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '.' || c == '_' || c == '-' || c == ' ' || c == '(' || c == ')' || c == '[' || c == ']') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) out = "upload.bin";
    return out;
}

std::string rom_sys_key_for_console(const CoreConfig &console)
{
    if (console.envCore && std::strcmp(console.envCore, "CP0_CORE_DC") == 0) {
        return "dc";
    }
    if (!console.romDirDefault || !console.romDirDefault[0]) return "";
    return lower_string(file_name_from_path(console.romDirDefault));
}

std::string normalize_rom_upload_sys_key(const std::string &sys)
{
    if (sys == "dreamcast") {
        return "dc";
    }
    return sys;
}

std::set<std::string> rom_upload_systems()
{
    std::set<std::string> out;
    for (const CoreConfig &c : g_consoles) {
        if (is_rom_upload_console(c) || is_settings_console(c)) continue;
        std::string key = rom_sys_key_for_console(c);
        if (!key.empty()) out.insert(key);
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> rom_upload_system_entries()
{
    std::vector<std::pair<std::string, std::string>> entries;
    std::set<std::string> used;
    for (const CoreConfig &c : g_consoles) {
        if (is_rom_upload_console(c) || is_settings_console(c)) continue;
        const std::string key = rom_sys_key_for_console(c);
        if (key.empty() || used.count(key)) continue;
        used.insert(key);
        entries.push_back({key, c.name});
    }
    return entries;
}

std::string unique_rom_path(const std::string &dir, const std::string &filename)
{
    std::string stem = filename;
    std::string ext;
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        stem = filename.substr(0, dot);
        ext = filename.substr(dot);
    }

    std::string candidate = join_path(dir, filename);
    int idx = 1;
    while (regular_file_exists(candidate)) {
        candidate = join_path(dir, stem + "_" + std::to_string(idx++) + ext);
    }
    return candidate;
}

void handle_rom_upload_client(int client_fd)
{
    std::string line;
    if (!read_http_line(client_fd, line)) return;

    std::istringstream req(line);
    std::string method;
    std::string target;
    std::string version;
    req >> method >> target >> version;
    if (method.empty() || target.empty()) {
        send_http_response(client_fd, "400 Bad Request", "text/plain", "Bad request");
        return;
    }

    std::map<std::string, std::string> headers;
    while (read_http_line(client_fd, line)) {
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = lower_string(trim_copy(line.substr(0, colon)));
        std::string val = trim_copy(line.substr(colon + 1));
        headers[key] = val;
    }

    if (method == "GET" && target == "/") {
        send_http_response(client_fd,
                           "200 OK",
                           "text/html; charset=utf-8",
                           build_rom_upload_webui_html(rom_upload_system_entries()));
        return;
    }

    if (method != "PUT") {
        send_http_response(client_fd, "404 Not Found", "text/plain", "Not found");
        return;
    }

    const size_t qpos = target.find('?');
    const std::string path = qpos == std::string::npos ? target : target.substr(0, qpos);
    const std::string query = qpos == std::string::npos ? "" : target.substr(qpos + 1);
    if (path != "/upload") {
        send_http_response(client_fd, "404 Not Found", "text/plain", "Not found");
        return;
    }

    auto params = parse_query(query);
    std::string sys = normalize_rom_upload_sys_key(lower_string(params["sys"]));
    std::string name = sanitize_filename(params["name"]);
    if (sys.empty()) {
        send_http_response(client_fd, "400 Bad Request", "text/plain", "Missing system");
        return;
    }
    const std::set<std::string> allowed = rom_upload_systems();
    if (allowed.find(sys) == allowed.end()) {
        send_http_response(client_fd, "400 Bad Request", "text/plain", "Unknown system");
        return;
    }
    if ((sys == "a2600" || sys == "a7800") && lower_ext(name) == ".zip") {
        send_http_response(client_fd,
                           "400 Bad Request",
                           "text/plain",
                           "Atari 2600/7800 ZIP not supported. Please unzip first and upload .a26, .a78 or .bin files.");
        return;
    }

    auto it_len = headers.find("content-length");
    if (it_len == headers.end()) {
        send_http_response(client_fd, "411 Length Required", "text/plain", "Missing Content-Length");
        return;
    }

    long long content_len = std::atoll(it_len->second.c_str());
    if (content_len <= 0) {
        send_http_response(client_fd, "400 Bad Request", "text/plain", "Empty upload");
        return;
    }

    const std::string dst_dir = "/home/pi/roms/" + sys;
    if (!ensure_dir_exists(dst_dir)) {
        send_http_response(client_fd, "500 Internal Server Error", "text/plain", "Cannot create destination dir");
        return;
    }
    const std::string dst_path = unique_rom_path(dst_dir, name);

    std::ofstream out(dst_path, std::ios::binary);
    if (!out.is_open()) {
        send_http_response(client_fd, "500 Internal Server Error", "text/plain", "Cannot open destination file");
        return;
    }

    long long remaining = content_len;
    std::array<char, 16384> buf {};
    while (remaining > 0) {
        const size_t want = static_cast<size_t>(std::min<long long>(remaining, static_cast<long long>(buf.size())));
        ssize_t n = recv(client_fd, buf.data(), want, 0);
        if (n <= 0) {
            out.close();
            unlink(dst_path.c_str());
            send_http_response(client_fd, "400 Bad Request", "text/plain", "Upload interrupted");
            return;
        }
        out.write(buf.data(), n);
        if (!out.good()) {
            out.close();
            unlink(dst_path.c_str());
            send_http_response(client_fd, "500 Internal Server Error", "text/plain", "Write failed");
            return;
        }
        remaining -= n;
    }

    send_http_response(client_fd,
                       "200 OK",
                       "text/plain",
                       "Uploaded into " + sys + ": " + file_name_from_path(dst_path));
}

void server_loop()
{
    while (g_server_running.load()) {
        sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }
        handle_rom_upload_client(client_fd);
        close(client_fd);
    }

    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    g_server_running.store(false);
}

bool start_server(std::string &error)
{
    error.clear();
    if (g_server_running.load()) {
        return true;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = std::string("socket() failed: ") + std::strerror(errno);
        return false;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    auto try_bind = [&](int port) {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(static_cast<uint16_t>(port));
        if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
            g_server_port = port;
            return true;
        }
        return false;
    };

    if (!try_bind(kPreferredPort)) {
        const int first_errno = errno;
        if (first_errno == EACCES && try_bind(kFallbackPort)) {
            // Fallback to a non-privileged port so upload works without sudo.
        } else {
            error = std::string("bind failed: ") + std::strerror(first_errno);
            close(fd);
            return false;
        }
    }
    if (listen(fd, 8) != 0) {
        error = std::string("listen failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    g_server_fd = fd;
    g_server_running.store(true);
    std::thread(server_loop).detach();
    return true;
}

} // namespace

namespace UploadServer {

// Reports whether the upload server background thread is active.
bool running()
{
    return g_server_running.load();
}

// Starts the ROM upload server if it is not already running.
StartResult start(const std::vector<CoreConfig> &consoles)
{
    StartResult result;
    g_consoles = consoles;

    std::string wifi_ip;
    if (!get_wifi_ipv4(wifi_ip)) {
        result.error = "Connect to Wi-Fi first, then retry.\nNeed an active wlan IP.";
        return result;
    }

    std::string error;
    if (!start_server(error)) {
        result.error = error;
        return result;
    }

    result.ok = true;
    result.port = g_server_port;
    result.message = "Open from phone/PC:\nhttp://" + wifi_ip +
        (g_server_port == 80 ? "" : (":" + std::to_string(g_server_port))) +
        "\n\nUpload one or multiple ROMs by system\n/home/pi/roms/<sys>";
    return result;
}

} // namespace UploadServer
