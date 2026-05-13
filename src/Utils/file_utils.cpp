#include "file_utils.h"

#include <cerrno>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <sys/stat.h>

std::string parent_dir(const std::string &path)
{
    if (path.empty() || path == "/") return "/";
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) return "/";
    return path.substr(0, slash);
}

std::string file_name_from_path(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return path;
    return path.substr(slash + 1);
}

std::string join_path(const std::string &dir, const std::string &name)
{
    if (dir.empty() || dir == "/") return "/" + name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

std::string lower_ext(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    if (slash != std::string::npos && dot < slash) return "";

    std::string ext = path.substr(dot);
    for (char &c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

bool ensure_dir_exists(const std::string &dir)
{
    if (dir.empty()) return false;

    std::string path;
    if (dir[0] == '/') path = "/";

    size_t start = (dir[0] == '/') ? 1u : 0u;
    while (start <= dir.size()) {
        size_t slash = dir.find('/', start);
        const std::string part = (slash == std::string::npos)
            ? dir.substr(start)
            : dir.substr(start, slash - start);
        if (!part.empty()) {
            if (!path.empty() && path.back() != '/') path += '/';
            path += part;

            struct stat st {};
            if (stat(path.c_str(), &st) == 0) {
                if (!S_ISDIR(st.st_mode)) return false;
            } else {
                if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
                    std::cerr << "file: mkdir failed for " << path
                              << ": " << std::strerror(errno) << "\n";
                    return false;
                }
            }
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

bool path_exists(const char *path)
{
    struct stat st {};
    return path && stat(path, &st) == 0;
}

bool regular_file_exists(const std::string &path)
{
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool read_file(const char *path, std::vector<uint8_t> &out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open ROM: " << path << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    if (size < 0) {
        std::cerr << "Failed to stat ROM: " << path << "\n";
        return false;
    }
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char *>(out.data()), size)) {
        std::cerr << "Failed to read ROM: " << path << "\n";
        return false;
    }
    return true;
}

bool file_matches_size(const std::string &path, size_t expected_size)
{
    struct stat st {};
    return stat(path.c_str(), &st) == 0 &&
           S_ISREG(st.st_mode) &&
           st.st_size == static_cast<off_t>(expected_size);
}

bool stat_regular_file(const char *path, size_t &size)
{
    struct stat st {};
    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    size = static_cast<size_t>(st.st_size);
    return true;
}

bool write_bytes_if_missing(const std::string &path, const uint8_t *data, size_t size)
{
    if (file_matches_size(path, size)) return true;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "file: failed to create " << path << ": "
                  << std::strerror(errno) << "\n";
        return false;
    }
    out.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
    if (!out) {
        std::cerr << "file: failed to write " << path << "\n";
        return false;
    }
    return true;
}

std::string shell_quote_local(const std::string &s)
{
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

std::string md5sum_file(const std::string &path)
{
    if (!regular_file_exists(path)) return "";
    const std::string cmd = "md5sum " + shell_quote_local(path);
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buf[64] {};
    std::string md5;
    if (fgets(buf, sizeof(buf), pipe)) {
        md5.assign(buf);
        size_t ws = md5.find_first_of(" \t\r\n");
        if (ws != std::string::npos) md5.resize(ws);
    }
    pclose(pipe);
    return md5;
}

bool copy_file_if_missing(const std::string &src, const std::string &dst)
{
    if (regular_file_exists(dst)) return true;
    if (!regular_file_exists(src)) return false;

    ensure_dir_exists(parent_dir(dst));
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!in || !out) {
        std::cerr << "file: failed to copy " << src
                  << " -> " << dst << "\n";
        return false;
    }
    out << in.rdbuf();
    if (!out) {
        std::cerr << "file: failed while writing " << dst << "\n";
        return false;
    }
    std::cout << "file: copied " << src << " -> " << dst << "\n";
    return true;
}

bool copy_file_overwrite_if_present(const std::string &src, const std::string &dst)
{
    if (!regular_file_exists(src)) return false;

    ensure_dir_exists(parent_dir(dst));
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!in || !out) {
        std::cerr << "file: failed to copy " << src
                  << " -> " << dst << "\n";
        return false;
    }
    out << in.rdbuf();
    if (!out) {
        std::cerr << "file: failed while writing " << dst << "\n";
        return false;
    }
    std::cout << "file: installed " << src << " -> " << dst << "\n";
    return true;
}

