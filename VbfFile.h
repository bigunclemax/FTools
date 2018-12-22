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
    uint32_t m_CRC32;
    std::string m_ascii_header;
    std::list <VbfBinarySection *> m_bin_sections;
    bool m_is_open = false;
public:
    bool IsOpen() const { return m_is_open;};
    int OpenFile (std::string file_name);
    int Export(std::string out_dir);
};


#endif //VBFEDIT_VBFFILE_H
