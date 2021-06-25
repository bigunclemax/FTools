//
// Created by bigun on 12/22/2018.
//
#ifdef __linux__
#include <arpa/inet.h>
#elif _WIN32
#include <winsock2.h>
#endif

#include <utility>
#include <CRC.h>
#include <sstream>
#include "VbfFile.h"
#include <utils.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

using namespace std;
using namespace rapidjson;
using std::runtime_error;

int VbfFile::OpenFile(const fs::path &file_path) {

    ifstream vbf_file(file_path, ios::binary | ios::ate);
    if(vbf_file.fail()) {
        throw runtime_error("Open VBF file error. '" + file_path.string() + "' " + string(strerror(errno)));
    }
    ifstream::pos_type pos = vbf_file.tellg();

    vector<uint8_t> file_buff(pos);
    vbf_file.seekg(0, ios::beg);
    vbf_file.read(reinterpret_cast<char *>(&file_buff[0]), file_buff.size());
    if (!vbf_file) {
        throw runtime_error("Read VBF file error. Only " + to_string(vbf_file.gcount()) + " bytes could be read");
    }
    vbf_file.close();

    m_file_name = fs::path(file_path).filename().string();
    m_file_length = file_buff.size();

    //start read ascii header and search first '{'
    m_ascii_header.clear();
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
        throw runtime_error("Parse VBF file error. ASCII header doesn't contain CRC32 checksum");
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
        throw runtime_error("Parse VBF file error. Binary data CRC32 mismatch");
    }

    //start read binary sections
    m_bin_sections.clear();
    auto i = data_section_offset;
    while (i < file_buff.size()) {

        auto section_offset = i;
        unique_ptr<VbfBinarySection> new_section(new VbfBinarySection());

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

        if (new_section->crc16 != crc) {
            cout << "Warn. VBF section at 0x" << std::hex << section_offset <<" CRC16 mismatch" << endl;
        }

        m_bin_sections.push_back(move(new_section));
    }

    m_is_open = true;
    return 0;
}

int VbfFile::Export(const fs::path &out_dir) {

    if (!IsOpen()) {
        return -1;
    }

    Document document;
    document.SetObject();
    Document::AllocatorType& allocator = document.GetAllocator();
    Value header(kObjectType);
    Value sections(kArrayType);

    string header_name = m_file_name + "_ascii_head.txt";
    FTUtils::bufferToFile(out_dir / header_name, &m_ascii_header[0], m_ascii_header.length());
    document.AddMember("header", Value(header_name.c_str(), allocator), allocator);

    int i = 0;
    for (const auto& section : m_bin_sections) {
        stringstream str_buff;
        str_buff << m_file_name << "_section_" << ++i << "_" << std::hex << section->start_addr << "_" << section->length << ".bin";

        FTUtils::bufferToFile(out_dir / str_buff.str(), reinterpret_cast<char *>(&section->data[0]), section->data.size());
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
    ofstream config(out_dir / (m_file_name + "_config.json"), ios::out);
    config.write(buffer.GetString(), buffer.GetSize());

    return 0;
}

int VbfFile::Import(const fs::path &conf_file_path) {

    fs::path config_path((fs::absolute(conf_file_path)));
    auto config_dir = config_path.parent_path();

    ifstream config_file(conf_file_path);
    if(config_file.fail()) {
        throw runtime_error("Import VBF file error. Can't open config file '" + conf_file_path.string() + "'");
    }

    string content((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
    Document document;
    document.Parse(content.c_str());

    Value& v = document["header"];
    string header_file_path = config_dir.string() + "/" + v.GetString();
    ifstream header_file(header_file_path, ios::binary);
    if(header_file.fail()) {
        throw runtime_error("Import VBF file error. Can't open header file '" + header_file_path + "'");
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
                throw runtime_error("Import VBF file error. Can't open VBF section file '" + vbf_section_path + "'");
            }
            streamsize vbf_section_size = vbf_section.tellg();
            vbf_section.seekg(0, ios::beg);

            vector<uint8_t> vbf_section_data(vbf_section_size);
            if (!vbf_section.read(reinterpret_cast<char *>(vbf_section_data.data()), vbf_section_size))
            {
                throw runtime_error("Import VBF file error. Can't read VBF section file '" + vbf_section_path + "'");
            }

            vbf_section.close();

            unique_ptr<VbfBinarySection> new_section(new VbfBinarySection());

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

            m_content_size += (new_section->length + sizeof(uint32_t)*2 + sizeof(uint16_t));
            m_bin_sections.push_back(move(new_section));
        }
    }
    m_CRC32 = crc32;
    m_file_name = "imported.vbf";
    m_ascii_header = vbf_header;
    m_is_open = true;

    return 0;
}

uint32_t VbfFile::calcCRC32() {

    uint32_t crc32 = 0;
    for(auto & section : m_bin_sections) {
        uint32_t length_be = htonl(*(uint32_t *)(&section->length));
        uint32_t addr_be = htonl(*(uint32_t *)(&section->start_addr));
        uint16_t crc16_be = htons(*(uint32_t *)(&section->crc16));

        crc32 = CRC::Calculate(&addr_be, sizeof(section->start_addr), CRC::CRC_32(), crc32);
        crc32 = CRC::Calculate(&length_be, sizeof(section->length), CRC::CRC_32(), crc32);
        crc32 = CRC::Calculate(section->data.data(), section->length, CRC::CRC_32(), crc32);
        crc32 = CRC::Calculate(&crc16_be, sizeof(section->crc16), CRC::CRC_32(), crc32);
    }
    return crc32;
}

int VbfFile::SaveToFile(const fs::path &file_path) {

    auto search_and_replace = [](std::string& str, const std::string& oldStr, const std::string& newStr) {
        std::string::size_type pos = 0u;
        while((pos = str.find(oldStr, pos)) != std::string::npos){
            str.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
    };

    // fix header
    m_CRC32 = calcCRC32();

    stringstream str_buff;
    str_buff << hex << setw(8) << setfill('0') << uppercase << m_CRC32;

    // change crc
    smatch m;
    regex_search(m_ascii_header, m, regex("\\bfile_checksum.*=.*0x(.*);"));
    if (m.size() != 2) {
        //FIXME: move this check to Import function
        throw runtime_error("Save VBF error. ASCII header not contain CRC32 checksum");
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
        throw runtime_error("Save VBF error. Can't create output file '" + file_path.string() + "'");
    }

    outfile.write(m_ascii_header.c_str(), m_ascii_header.length());

    for(const auto& section :m_bin_sections)
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
    m_content_size += (section_data.size() - (*sections_it)->length);
    (*sections_it)->data = section_data;
    (*sections_it)->length = section_data.size();
    (*sections_it)->crc16 = CRC::Calculate(section_data.data(), section_data.size(), CRC::CRC_16_CCITTFALSE());

    return 0;
}

int VbfFile::GetSectionInfo(uint8_t section_idx, VbfFile::SectionInfo &info) {

    if(section_idx >= m_bin_sections.size()) {
        return -1;
    }

    auto sections_it = m_bin_sections.begin();
    std::advance(sections_it, section_idx);
    info.start_addr = (*sections_it)->start_addr;
    info.length = (*sections_it)->length;

    return 0;
}
