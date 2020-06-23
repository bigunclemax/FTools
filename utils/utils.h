//
// Created by user on 26.05.2020.
//

#ifndef FORDTOOLS_UTILS_H
#define FORDTOOLS_UTILS_H

#include <string>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace FTUtils {

    inline void bufferToFile(const fs::path& file_name, const char *data_ptr, int data_len) {

        std::ofstream out_file(file_name, std::ios::out | std::ios::binary);
        if (!out_file) {
            throw std::runtime_error("file " + file_name.string() + " can't be created");
        } else {
            out_file.write(data_ptr, data_len);
        }
        out_file.close();
    }

    inline void vectorToFile(const fs::path& file_name, const std::vector<uint8_t>& data) {

        std::ofstream out_file(file_name, std::ios::out | std::ios::binary);
        if (!out_file) {
            throw std::runtime_error("file " + file_name.string() + " can't be created");
        } else {
            out_file.write((const char*)data.data(), data.size());
        }
        out_file.close();
    }

    inline void fileToVector(const fs::path& file_path, std::vector<uint8_t>& data) {

        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if(file.fail()) {
            throw std::runtime_error("Can't open file '" + file_path.string() + "'");
        }
        auto file_sz = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(file_sz);
        if (!file.read(reinterpret_cast<char *>(data.data()), file_sz)) {
            throw std::runtime_error("Can't read file '" + file_path.string() + "'");
        }

        file.close();
    }

    inline std::vector<uint8_t> fileToVector(const fs::path& file_path) {

        std::vector<uint8_t> data;

        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if(file.fail()) {
            throw std::runtime_error("Can't open file '" + file_path.string() + "'");
        }
        auto file_sz = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(file_sz);
        if (!file.read(reinterpret_cast<char *>(data.data()), file_sz)) {
            throw std::runtime_error("Can't read file '" + file_path.string() + "'");
        }

        file.close();

        return data;
    }

}

#endif //FORDTOOLS_UTILS_H
