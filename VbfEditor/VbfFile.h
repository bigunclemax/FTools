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
    std::list <VbfBinarySection *> m_bin_sections;
    bool m_is_open = false;
public:
    [[nodiscard]] bool IsOpen() const { return m_is_open;};
    int OpenFile (const std::string& file_path);
    int SaveToFile (std::string file_path);
    int Export(const std::string& out_dir);
    int Import(const std::string& conf_file_path);

    int GetSectionRaw(uint8_t section_idx, std::vector<uint8_t> section_data);
};


#endif //VBFEDIT_VBFFILE_H
