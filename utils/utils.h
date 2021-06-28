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

    inline std::string path2c_str(const fs::path &file_path) {
#ifdef _WIN32
        const auto &ws = file_path.wstring();
        std::setlocale(LC_ALL, "");
        const std::locale locale("");
        typedef std::codecvt<wchar_t, char, std::mbstate_t> converter_type;
        const auto& converter = std::use_facet<converter_type>(locale);
        std::vector<char> to(ws.length() * converter.max_length());
        std::mbstate_t state;
        const wchar_t* from_next;
        char* to_next;
        const converter_type::result result = converter.out(state, ws.data(), ws.data() + ws.length(), from_next, &to[0], &to[0] + to.size(), to_next);
        if (result == converter_type::ok || result == converter_type::noconv) {
            return std::string (&to[0], to_next);
        }
        throw std::runtime_error("WString converting error");
#else
        return file_path.string();
#endif
    }
}

#endif //FORDTOOLS_UTILS_H
