//
// Created by bigun on 12/22/2018.
//
#ifdef __linux__
#include <arpa/inet.h>
#elif _WIN32

#include <winsock2.h>
#endif

#include <CRC.h>
#include "VbfFile.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

using namespace std;
using namespace rapidjson;

int VbfFile::OpenFile(string file_name) {

    ifstream vbf_file(file_name, ios::binary | ios::ate);
    ifstream::pos_type pos = vbf_file.tellg();

    vector<uint8_t> file_buff(pos);
    vbf_file.seekg(0, ios::beg);
    vbf_file.read(reinterpret_cast<char *>(&file_buff[0]), file_buff.size());
    if (!vbf_file) {
        cout << "Read VBF file error, only " << vbf_file.gcount() << " could be read" << endl;
        vbf_file.close();
        return -1;
    }
    vbf_file.close();

    auto getFileName = [](string &file_name) {
        char sep = '/';
#ifdef _WIN32
        sep = '\\';
#endif
        size_t i = file_name.rfind(sep, file_name.length());
        if (i != string::npos) {
            return (file_name.substr(i + 1, file_name.length() - i));
        }

        return file_name;
    };

    m_file_name = getFileName(file_name);
    m_file_length = file_buff.size();

    cout << "Parse VBF file " << m_file_name << " Size: " << m_file_length << " bytes" << endl;

    //start read ascii header and search first '{'
    auto opened_brackets = 0;
    for (auto symbol : file_buff) {
        m_ascii_header.push_back(symbol);
        if (symbol == '{') {
            opened_brackets++;
        }
        if (symbol == '}') {
            opened_brackets--;
            if (!opened_brackets) {
                break;
            }
        }
    }

    //try to find file checksum
    smatch m;
    regex e("\\bfile_checksum.*=.*0x(.*);");

    regex_search(m_ascii_header, m, e);
    if (m.size() != 2) {
        cout << "VBF ascii header not contain CRC32 checksum" << endl;
        return -1;
    }
    m_CRC32 = stoul(m[1], nullptr, 16);

    //check crc32 of whole binary data section
    auto data_section_offset = m_ascii_header.length();
    auto binary_data_len = file_buff.size() - data_section_offset;

    auto crc32 = CRC::Calculate(&file_buff[data_section_offset], binary_data_len, CRC::CRC_32());
    if (m_CRC32 != crc32) {
        cout << "VBF binary data wrong checksum " << endl;
        return -1;
    }

    cout << "VBF data CRC - [correct]" << endl << endl;

    //start read binary sections
    auto i = data_section_offset;
    while (i < file_buff.size()) {

        auto *new_section = new VbfBinarySection();
        new_section->start_addr = ntohl(*reinterpret_cast<uint32_t *>(&file_buff[i]));
        i += sizeof(uint32_t);
        new_section->length = ntohl(*reinterpret_cast<uint32_t *>(&file_buff[i]));
        i += sizeof(uint32_t);

        new_section->data.resize(new_section->length);
        copy(file_buff.begin() + i, file_buff.begin() + i + new_section->length, new_section->data.begin());
        i += new_section->length;

        new_section->crc16 = ntohs(*reinterpret_cast<uint16_t *>(&file_buff[i]));
        i += sizeof(uint16_t);

        auto crc = CRC::Calculate(&new_section->data[0], new_section->length, CRC::CRC_16_CCITTFALSE());

        cout << std::hex << "### Got section ###" << endl
             << "Section start addr: 0x" << new_section->start_addr << endl
             << "Length: 0x" << new_section->length << endl
             << "CRC: 0x" << crc << ((new_section->crc16 == crc) ? " [correct]" : " [ERROR]") << endl << endl;

        m_bin_sections.push_back(new_section);
    }

    m_is_open = true;
    return 0;
}

int VbfFile::Export(string out_dir) {

    if (!IsOpen()) {
        return -1;
    }

    auto SaveToFile = [](string file_name, char *data_ptr, int data_len) {
        ofstream out_file(file_name, ios::out | ios::binary);
        if (!out_file) {
            cout << "file: " << file_name << " can't be created" << endl;
        } else {
            out_file.write(data_ptr, data_len);
        }
        out_file.close();
    };

    stringstream str_buff;
    str_buff << out_dir << m_file_name << "_ascii_head.txt";
    SaveToFile(str_buff.str(), &m_ascii_header[0], m_ascii_header.length());

    for (auto section : m_bin_sections) {
        str_buff.str(string());
        str_buff.clear();
        str_buff << std::hex << out_dir << m_file_name << "_section_"
                 << section->start_addr << "_" << section->length << ".bin";

        SaveToFile(str_buff.str(), reinterpret_cast<char *>(&section->data[0]), section->data.size());
    }

    //create json
    Document document;
    Document::AllocatorType& allocator = document.GetAllocator();

    Value obj(kObjectType);
    {
        Value header(kObjectType);
        {
            Value file(kStringType);
            header.AddMember("file", file, allocator);
        }

        Value sections(kArrayType);
        {
            Value section(kObjectType);
            {
                Value file(kStringType);
                section.AddMember("file", file, allocator);

                Value address(kNumberType);
                section.AddMember("address", address, allocator);
            }
            sections.PushBack(section, allocator);
        }
        obj.AddMember("header", header, allocator);
        obj.AddMember("sections", sections, allocator);
    }

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    document.Accept(writer);

    return 0;
}

int VbfFile::Import(string descr_file) {

    return -1;
}