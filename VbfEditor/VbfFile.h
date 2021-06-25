//
// Created by bigun on 12/22/2018.
//

#ifndef VBFEDIT_VBFFILE_H
#define VBFEDIT_VBFFILE_H

#include <iostream>
#include <cstring>
#include <vector>
#include <list>
#include <fstream>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

struct VbfBinarySection {
    uint32_t start_addr;
    uint32_t length;
    std::vector<uint8_t> data;
    uint16_t crc16;
};

class VbfFile {
    std::string m_file_name;
    uint32_t m_file_length;
    uint32_t m_CRC32;           //whole binary data CRC
    uint32_t m_content_size;    //whole binary data size in bytes
    std::string m_ascii_header;
    std::list <std::unique_ptr<VbfBinarySection>> m_bin_sections;
    bool m_is_open = false;

    uint32_t calcCRC32();
public:

    struct SectionInfo {
        uint32_t start_addr;
        uint32_t length;
    };

    [[nodiscard]] bool IsOpen() const { return m_is_open;};
    int OpenFile (const fs::path &file_path);
    int SaveToFile (const fs::path &file_path);
    int Export(const fs::path &out_dir);
    int Import(const fs::path &conf_file_path);

    int GetSectionInfo(uint8_t section_idx, SectionInfo &info);
    int GetSectionRaw(uint8_t section_idx, std::vector<uint8_t>& section_data);
    int ReplaceSectionRaw(uint8_t section_idx,const std::vector<uint8_t>& section_data);

    inline int SectionsCount() { return m_bin_sections.size();};
    inline int HeaderSz() { return m_ascii_header.length();};
};


#endif //VBFEDIT_VBFFILE_H
