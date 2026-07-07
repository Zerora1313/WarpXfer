#include "../include/folder_scanner.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace fileio {

std::string NormalizePath(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

std::string GetDirectoryOfPath(const std::string& filepath) {
    std::string norm = NormalizePath(filepath);
    size_t last_slash = norm.find_last_of('/');
    if (last_slash == std::string::npos) {
        return "";
    }
    return norm.substr(0, last_slash);
}

bool CreateDirectoryRecursive(const std::string& dir_path) {
    try {
        fs::create_directories(fs::path(dir_path));
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "CreateDirectoryRecursive error: " << e.what() << std::endl;
        return false;
    }
}

ScanResult ScanFolderOrFile(const std::string& path) {
    ScanResult result;
    std::string norm = NormalizePath(path);
    if (norm.empty()) return result;

    fs::path p = fs::absolute(fs::path(norm));
    if (!fs::exists(p)) {
        return result;
    }

    result.root_name = p.filename().string();

    if (fs::is_directory(p)) {
        result.is_single_file = false;
        fs::path parent_path = p.parent_path();

        try {
            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_directory()) {
                    result.total_folders++;
                } else if (entry.is_regular_file()) {
                    result.total_files++;
                    uint64_t file_size = fs::file_size(entry.path());
                    result.total_size += file_size;

                    FileInfo info;
                    std::string rel_path = fs::relative(entry.path(), parent_path).string();
                    info.relative_path = NormalizePath(rel_path);
                    info.absolute_path = NormalizePath(entry.path().string());
                    info.size = file_size;
                    result.files.push_back(info);

                    std::string ext = entry.path().extension().string();
                    if (!ext.empty()) {
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        result.type_counts[ext]++;
                    } else {
                        result.type_counts[".no-ext"]++;
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "ScanFolderOrFile error: " << e.what() << std::endl;
        }
    } else {
        result.is_single_file = true;
        result.total_files = 1;
        try {
            uint64_t file_size = fs::file_size(p);
            result.total_size = file_size;

            FileInfo info;
            info.relative_path = result.root_name;
            info.absolute_path = norm;
            info.size = file_size;
            result.files.push_back(info);

            std::string ext = p.extension().string();
            if (!ext.empty()) {
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                result.type_counts[ext]++;
            } else {
                result.type_counts[".no-ext"]++;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "ScanFolderOrFile file error: " << e.what() << std::endl;
        }
    }

    return result;
}

} // namespace fileio
