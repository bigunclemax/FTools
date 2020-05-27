//
// Created by bigun on 12/22/2018.
//
#ifdef __linux__
#include <arpa/inet.h>
#elif _WIN32
#include <winsock2.h>
#endif

#include <filesystem>
#include <utility>
#include <CRC.h>
#include <sstream>
#include "VbfFile.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

using namespace std;
using namespace rapidjson;
namespace fs = std::filesystem;

int VbfFile::OpenFile(const string& file_path) {

    ifstream vbf_file(file_path, ios::binary | ios::ate);
    if(vbf_file.fail()) {
        cout << "Open VBF file error" << endl;
        return -1;
    }
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

    m_file_name = fs::path(file_path).filename().string();
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
    regex_search(m_ascii_header, m, regex("\\bfile_checksum.*=.*0x(.*);"));
    if (m.size() != 2) {
        cout << "VBF ascii header not contain CRC32 checksum" << endl;
        return -1;
    }
    m_CRC32 = stoul(m[1], nullptr, 16);

    //check crc32 of whole binary data section
    auto data_section_offset = m_ascii_header.length();
    m_content_size = file_buff.size() - data_section_offset;

    // try find content size in header
    regex_search(m_ascii_header, m, regex("\\bBytes:.*?(\\d+)"));
    if (m.size() != 2) {
        cout << "Warn. VBF ascii header not contain content length" << endl;
    } else {
        if(m_content_size != (stoi(m[1]) + 20)) //NOTE: i don't know why but real content size 20 bytes more then header Bytes value
        {
            cout << "Warn. VBF ascii header content length and real content length mismatch" << endl;
        }
    }

    auto crc32 = CRC::Calculate(&file_buff[data_section_offset], m_content_size, CRC::CRC_32());
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

int VbfFile::Export(const string& out_dir) {

    if (!IsOpen()) {
        return -1;
    }

    auto SaveToFile = [](const string& file_name, char *data_ptr, int data_len) {
        ofstream out_file(file_name, ios::out | ios::binary);
        if (!out_file) {
            cout << "file: " << file_name << " can't be created" << endl;
        } else {
            out_file.write(data_ptr, data_len);
        }
        out_file.close();
    };

    Document document;
    document.SetObject();
    Document::AllocatorType& allocator = document.GetAllocator();
    Value header(kObjectType);
    Value sections(kArrayType);

    string header_name = m_file_name + "_ascii_head.txt";
    SaveToFile(out_dir + header_name, &m_ascii_header[0], m_ascii_header.length());
    document.AddMember("header", Value(header_name.c_str(), allocator), allocator);

    for (auto section : m_bin_sections) {
        stringstream str_buff;
        str_buff << m_file_name << "_section_" << std::hex << section->start_addr << "_" << section->length << ".bin";

        SaveToFile(out_dir + str_buff.str(), reinterpret_cast<char *>(&section->data[0]), section->data.size());
        Value section_obj(kObjectType);
        {
            section_obj.AddMember("file", Value(str_buff.str().c_str(), allocator), allocator);
            stringstream _addr_hex;
            _addr_hex << "0x" << std::hex << section->start_addr;
            section_obj.AddMember("address", Value(_addr_hex.str().c_str(), allocator), allocator);
        }
        sections.PushBack(section_obj, allocator);
    }
    document.AddMember("sections", sections, allocator);

    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);
    ofstream config(out_dir + m_file_name + "_config.json", ios::out);
    config.write(buffer.GetString(), buffer.GetSize());

    return 0;
}

int VbfFile::Import(const string& conf_file_path) {

    fs::path config_path((fs::absolute(conf_file_path)));
    auto config_dir = config_path.parent_path();

    ifstream config_file(conf_file_path);
    if(config_file.fail()) {
        return -1;
    }

    string content((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
    Document document;
    document.Parse(content.c_str());

    Value& v = document["header"];
    string header_file_path = config_dir.string() + "/" + v.GetString();
    ifstream header_file(header_file_path, ios::binary);
    if(header_file.fail()) {
        cerr << "Can't open header " << header_file_path << endl;
        return -1;
    }
    string vbf_header((istreambuf_iterator<char>(header_file)), istreambuf_iterator<char>());
    //TODO: validate header

    m_content_size =0;
    uint32_t crc32 = 0;
    {
        const Value& sections = document["sections"];
        assert(sections.IsArray());
        for (SizeType i = 0; i < sections.Size(); ++i) {
            auto sec_obj = sections[i].GetObject();
            const Value& file = sec_obj["file"];
            const Value& address = sec_obj["address"];

            string address_str = address.GetString();
            uint32_t vbf_section_addr = strtoul(address_str.c_str(), nullptr, 16);

            string vbf_section_path = config_dir.string() + "/" +file.GetString();
            ifstream vbf_section(vbf_section_path, ios::binary | std::ios::ate);
            if(vbf_section.fail()) {
                cerr << "Can't open vbf section " << vbf_section_path << endl;
            }
            streamsize vbf_section_size = vbf_section.tellg();
            vbf_section.seekg(0, ios::beg);

            vector<uint8_t> vbf_section_data(vbf_section_size);
            if (!vbf_section.read(reinterpret_cast<char *>(vbf_section_data.data()), vbf_section_size))
                cerr << "Can't read vbf section file" << endl;

            vbf_section.close();

            auto *new_section = new VbfBinarySection();
            new_section->start_addr = vbf_section_addr;
            new_section->length = vbf_section_size;
            new_section->data = vbf_section_data;
            new_section->crc16 = CRC::Calculate(new_section->data.data(), new_section->length, CRC::CRC_16_CCITTFALSE());

            uint32_t length_be = htonl(*reinterpret_cast<uint32_t *>(&new_section->length));
            uint32_t addr_be = htonl(*reinterpret_cast<uint32_t *>(&new_section->start_addr));
            uint16_t crc16_be = htons(*reinterpret_cast<uint16_t *>(&new_section->crc16));

            crc32 = CRC::Calculate(&addr_be, sizeof(new_section->start_addr), CRC::CRC_32(), crc32);
            crc32 = CRC::Calculate(&length_be, sizeof(new_section->length), CRC::CRC_32(), crc32);
            crc32 = CRC::Calculate(new_section->data.data(), new_section->length, CRC::CRC_32(), crc32);
            crc32 = CRC::Calculate(&crc16_be, sizeof(new_section->crc16), CRC::CRC_32(), crc32);

            m_bin_sections.push_back(new_section);
            m_content_size += (new_section->length + sizeof(uint32_t)*2 + sizeof(uint16_t));
        }
    }
    m_CRC32 = crc32;
    m_file_name = "imported.vbf";
    m_ascii_header = vbf_header;
    m_is_open = true;

    return -1;
}

int VbfFile::SaveToFile(std::string file_path) {

    if(file_path.empty()) {
        file_path = m_file_name;
    }

    auto search_and_replace = [](std::string& str, const std::string& oldStr, const std::string& newStr) {
        std::string::size_type pos = 0u;
        while((pos = str.find(oldStr, pos)) != std::string::npos){
            str.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
    };

    //header magic
    stringstream str_buff;
    str_buff << hex << setw(8) << setfill('0') << uppercase << m_CRC32;

    // change crc
    smatch m;
    regex_search(m_ascii_header, m, regex("\\bfile_checksum.*=.*0x(.*);"));
    if (m.size() != 2) {
        cout << "VBF ascii header not contain CRC32 checksum" << endl;
        return -1;
    }
    search_and_replace(m_ascii_header, m[1], str_buff.str());

    // change length
    regex_search(m_ascii_header, m, regex("// Bytes:    \\d+"));
    if(!m.empty()) {
        stringstream ss ;
        ss << "// Bytes:    " << m_content_size - 20; //backward fix
        search_and_replace(m_ascii_header, m[0], ss.str());
    }

    // create out file
    ofstream outfile(file_path, ios::binary | ios::out);
    if(outfile.fail()) {
        cerr << "Can't create out file " << file_path << endl;
        return -1;
    }

    outfile.write(m_ascii_header.c_str(), m_ascii_header.length());

    for(auto section :m_bin_sections)
    {
        uint32_t length_be = htonl(*reinterpret_cast<uint32_t *>(&section->length));
        uint32_t addr_be = htonl(*reinterpret_cast<uint32_t *>(&section->start_addr));
        uint16_t crc16_be = htons(*reinterpret_cast<uint16_t *>(&section->crc16));

        outfile.write(reinterpret_cast<char *>(&addr_be), sizeof(addr_be));
        outfile.write(reinterpret_cast<char *>(&length_be), sizeof(length_be));
        outfile.write(reinterpret_cast<char *>(section->data.data()), section->data.size());
        outfile.write(reinterpret_cast<char *>(&crc16_be), sizeof(crc16_be));
    }

    return 0;
}

int VbfFile::GetSectionRaw(uint8_t section_idx, std::vector<uint8_t>& section_data) {

    if(section_idx >= m_bin_sections.size()) {
        return -1;
    }

    auto sections_it = m_bin_sections.begin();
    std::advance(sections_it, section_idx);
    section_data = (*sections_it)->data;

    return 0;
}

int VbfFile::ReplaceSectionRaw(uint8_t section_idx, const vector<uint8_t> &section_data) {

    if(section_idx >= m_bin_sections.size()) {
        return -1;
    }

    auto sections_it = m_bin_sections.begin();
    std::advance(sections_it, section_idx);
    (*sections_it)->data = section_data;
    (*sections_it)->length = section_data.size();
    (*sections_it)->crc16 = CRC::Calculate(section_data.data(), section_data.size(), CRC::CRC_16_CCITTFALSE());

    return 0;
}
