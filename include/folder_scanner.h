#ifndef FOLDER_SCANNER_H
#define FOLDER_SCANNER_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace fileio {

struct FileInfo {
    std::string relative_path; // e.g. "subfolder/file.txt"
    std::string absolute_path; // e.g. "D:/schoolProject/subfolder/file.txt"
    uint64_t size;             // size in bytes
};

struct ScanResult {
    std::string root_name;                     // e.g. "schoolProject"
    std::vector<FileInfo> files;
    uint64_t total_size = 0;
    uint32_t total_files = 0;
    uint32_t total_folders = 0;
    std::map<std::string, uint32_t> type_counts; // e.g. {".cpp": 12, ".h": 8}
    bool is_single_file = false;
};

ScanResult ScanFolderOrFile(const std::string& path);
bool CreateDirectoryRecursive(const std::string& dir_path);
std::string GetDirectoryOfPath(const std::string& filepath);
std::string NormalizePath(const std::string& path);

} // namespace fileio

#endif // FOLDER_SCANNER_H
