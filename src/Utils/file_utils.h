#ifndef CP0_FILE_UTILS_H
#define CP0_FILE_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Generic filesystem helpers shared by the runtime, UI, downloader, and BIOS code.
std::string parent_dir(const std::string &path);
std::string file_name_from_path(const std::string &path);
std::string join_path(const std::string &dir, const std::string &name);
std::string lower_ext(const std::string &path);
bool ensure_dir_exists(const std::string &dir);
bool path_exists(const char *path);
bool regular_file_exists(const std::string &path);
bool read_file(const char *path, std::vector<uint8_t> &out);
bool file_matches_size(const std::string &path, size_t expected_size);
bool stat_regular_file(const char *path, size_t &size);
bool write_bytes_if_missing(const std::string &path, const uint8_t *data, size_t size);
std::string md5sum_file(const std::string &path);
bool copy_file_if_missing(const std::string &src, const std::string &dst);
bool copy_file_overwrite_if_present(const std::string &src, const std::string &dst);

#endif // CP0_FILE_UTILS_H
