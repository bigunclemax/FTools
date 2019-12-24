//
// Created by bigun on 12/20/2018.
//

#include <iostream>
#include <fstream>
#include <streambuf>
#include <list>
#include <regex>
#include <iomanip>
#include <miniz_zip.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "CRC.h"
#include <cxxopts.hpp>

using namespace std;
using namespace rapidjson;


#define START_ZIP_SECTION 0x9094


struct zip_file {
    uint32_t width;
    uint32_t height;
    uint8_t  img_type;
    char     fileName[31];
};

struct ttf_file {
    char fileName[32];
};

#if 0
struct extracted_item {
    uint32_t unk1;
    uint32_t unk2;
    uint8_t  unk3;
    array<char, 32> ascii_string;
    uint32_t size;
    vector<uint8_t> data;
};
#endif

enum image_type : uint8_t {
    EIF_TYPE_MONOCHROME = 0x04,
    EIF_TYPE_MULTICOLOR = 0x07,
    EIF_TYPE_SUPERCOLOR = 0x0E
};

inline const char* ToString(image_type v)
{
    switch (v)
    {
        case EIF_TYPE_MONOCHROME: return "MONOCHROME";
        case EIF_TYPE_MULTICOLOR: return "MULTICOLOR";
        case EIF_TYPE_SUPERCOLOR: return "SUPERCOLOR";
        default:      return "[Unknown] ";
    }
}
//vector<struct extracted_item>& extracted_items

auto SaveToFile(const string& file_name, char *data_ptr, int data_len) {
    ofstream out_file(file_name, ios::out | ios::binary);
    if (!out_file) {
        cout << "file: " << file_name << " can't be created" << endl;
    } else {
        out_file.write(data_ptr, data_len);
    }
    out_file.close();
}

int parsePicturesSection(const string& file_path,
        const string& out_path_pref, bool verbose, bool need_extract)
{

    ifstream vbf_file(file_path, ios::binary | ios::ate);
    if(!vbf_file){
        cout << "Input file not found" << endl;
        return -1;
    }
    ifstream::pos_type pos = vbf_file.tellg();

    vbf_file.seekg(0, ios::beg);
    vector<uint8_t> file_buff(pos);

    vbf_file.read(reinterpret_cast<char*>(&file_buff[0]), file_buff.size());
    if(!vbf_file){
        cout << "Read section error, only " << vbf_file.gcount() << " could be read" << endl;
        return -1;
    }
    vbf_file.close();
    cout << "Read 0x" << std::hex << pos << " bytes" << resetiosflags(ios::hex) << endl;

    auto read_idx = START_ZIP_SECTION;
    // parse Zip headers block
    uint32_t itemsCount = *reinterpret_cast<uint32_t *>(&file_buff[read_idx]);
    read_idx += sizeof(uint32_t);
    auto zip_headers_offset = &file_buff[read_idx];
    read_idx += sizeof(struct zip_file) * itemsCount;
    auto zip_count = itemsCount;
    cout << "ZIP Items count " << itemsCount << endl;

    // parse TTF headers block
    itemsCount = *reinterpret_cast<uint32_t *>(&file_buff[read_idx]);
    read_idx += sizeof(uint32_t);
    auto ttf_headers_offset = &file_buff[read_idx];
    read_idx += sizeof(struct ttf_file) * itemsCount;
    auto ttf_count = itemsCount;
    cout << "TTF Items count " << itemsCount << endl;

    auto magicInt =  *reinterpret_cast<uint32_t *>(&file_buff[read_idx]);
    read_idx += sizeof(uint32_t);
    cout << "unknown num 0x" << hex << magicInt << resetiosflags(ios::hex) << endl;
    cout << "data offset is 0x" << hex << read_idx << resetiosflags(ios::hex) << endl;

    Document document;
    document.SetObject();
    Document::AllocatorType& allocator = document.GetAllocator();
    Value img_sections(kArrayType);
    Value ttf_sections(kArrayType);
    if(need_extract) {
        stringstream str_buff;
        str_buff << "_header.bin";

        SaveToFile(out_path_pref + str_buff.str(),
                   reinterpret_cast<char *>(file_buff.data()),read_idx);

        document.AddMember("header", Value(str_buff.str().c_str(), allocator), allocator);
    }

    if(verbose) {
        cout
        << "#####################" << endl
        << "#### ZIP section ####" << endl
        << "#####################" << endl;

        cout    << "   # "
                << " | "
                << "          ASCII string          "
                << " | "
                << "  Size  "
                << " | "
                << "   Type   "
                << " | "
                << "  W   "
                << " | "
                << "  H   "
                << " | "
                << " Note "
                << endl;
    }

    auto zip_header_ptr = reinterpret_cast<struct zip_file *>(zip_headers_offset);
    for(int i=0; i < zip_count; ++i) {

        auto zip_header = zip_header_ptr[i];

        // get actual size and offset
        smatch m;
        string _str(zip_header.fileName);
        regex_search(_str, m, regex("(\\d+).zip"));
        if (m.size() != 2) {
            cout << "zip header name not contain actual size!" << endl;
            return -1;
        }
        auto actual_sz = stoul(m[1].str().c_str(), nullptr, 10);
        auto _addr = stoul(&zip_header.fileName[5], nullptr, 16);
#if 0
        //get real size
        unsigned long _addr_next;
        char * next_addr_str;
        if(i != (zip_count-1)) {
            next_addr_str = &zip_header_ptr[i+1].fileName[5];
        } else {
            next_addr_str = &(*reinterpret_cast<struct ttf_file *>(ttf_headers_offset)).fileName[5];
        }

        _addr_next = stoul(next_addr_str, nullptr, 16);
        auto real_sz = _addr_next - _addr;
#endif
        //get relative offset
        auto relative_offset =  _addr - 0x02400000;
        auto remainder = actual_sz % 4;
        auto padded_sz = actual_sz + ((remainder) ? (4 - remainder) : 0);

        if(verbose) {
            mz_zip_archive zip_archive;
            memset(&zip_archive, 0, sizeof(zip_archive));
            if (!mz_zip_reader_init_mem(&zip_archive, reinterpret_cast<const void *>(&file_buff[relative_offset]),
                                        actual_sz, 0)) {
                cerr << "Can't initialize archive";
                return -1;
            }
            char _fileName[32];
            mz_zip_reader_get_filename(&zip_archive, 0, _fileName, 32);
            mz_zip_reader_end(&zip_archive);

            cout << resetiosflags(ios::hex) << std::setfill(' ')
                << setw(5) << i
                << " | "
                << setiosflags(ios::left) << setw(32) << zip_header.fileName << resetiosflags(ios::left)
                << " | "
                << setw(8) << padded_sz
                << " | "
                << ToString(static_cast<image_type>(zip_header.img_type))
                << " | "
                << setw(5) << zip_header.width << " "
                << " | "
                << setw(5) << zip_header.height << " "
                << " | "
                << _fileName
                << endl;
        }

        if(need_extract) {

            stringstream str_buff;
            str_buff << i << ".zip";

            SaveToFile(out_path_pref + str_buff.str(),
                       reinterpret_cast<char *>(&file_buff[relative_offset]), actual_sz);

            Value section_obj(kObjectType);
            {
                section_obj.AddMember("file", Value(str_buff.str().c_str(), allocator), allocator);
                stringstream _addr_hex;
                _addr_hex << "0x" << std::hex << relative_offset;
                section_obj.AddMember("relative-offset", Value(_addr_hex.str().c_str(), allocator), allocator);
                section_obj.AddMember("actual-size", actual_sz, allocator);
                section_obj.AddMember("padded-size", padded_sz, allocator);
                section_obj.AddMember("width", zip_header.width, allocator);
                section_obj.AddMember("height", zip_header.height, allocator);
                section_obj.AddMember("type", Value(ToString(static_cast<image_type>(zip_header.img_type)), allocator), allocator);
            }
            img_sections.PushBack(section_obj, allocator);
        }
    }

    cout
            << "#####################" << endl
            << "#### TTF section ####" << endl
            << "#####################" << endl;

    cout    << "   # "
            << " | "
            << "          ASCII string          "
            << " | "
            << "  Size  "
            << endl;

    auto ttf_header_ptr = reinterpret_cast<struct ttf_file *>(ttf_headers_offset);
    for(int i=0; i < ttf_count; ++i) {

        auto ttf_header = ttf_header_ptr[i];

        // get actual size and offset
        smatch m;
        string _str(ttf_header.fileName);
        regex_search(_str, m, regex("(\\d+).ttf"));
        if (m.size() != 2) {
            cout << "ttf header name not contain actual size!" << endl;
            return -1;
        }
        auto actual_sz = stoul(m[1].str().c_str(), nullptr, 10);
        auto _addr = stoul(&ttf_header.fileName[5], nullptr, 16);
#if 0
        //get real size
        unsigned long _addr_next;
        char * next_addr_str;
        if(i != (ttf_count-1)) {
            next_addr_str = &ttf_header_ptr[i+1].fileName[5];
            _addr_next = stoul(next_addr_str, nullptr, 16);
        } else {
            _addr_next = file_buff.size() + 0x02400000;
        }
        auto real_sz = _addr_next - _addr;
#endif
        //get relative offset
        auto relative_offset =  _addr - 0x02400000;
        auto remainder = actual_sz % 4;
        auto padded_sz = actual_sz + ((remainder) ? (4 - remainder) : 0);

        if(verbose) {
            cout << resetiosflags(ios::hex)  << std::setfill(' ')
                 << setw(5) << i
                 << " | "
                 << setiosflags(ios::left) << setw(32) << ttf_header.fileName << resetiosflags(ios::left)
                 << " | "
                 << setw(8) << padded_sz
                 << endl;
        }

        if(need_extract) {

            stringstream str_buff;
            str_buff << i << ".ttf";

            SaveToFile(out_path_pref + str_buff.str(),
                       reinterpret_cast<char *>(&file_buff[relative_offset]), actual_sz);

            Value section_obj(kObjectType);
            {
                section_obj.AddMember("file", Value(str_buff.str().c_str(), allocator), allocator);
                stringstream _addr_hex;
                _addr_hex << "0x" << std::hex << relative_offset;
                section_obj.AddMember("relative-offset", Value(_addr_hex.str().c_str(), allocator), allocator);
                section_obj.AddMember("actual-size", actual_sz, allocator);
                section_obj.AddMember("padded-size", padded_sz, allocator);
            }
            ttf_sections.PushBack(section_obj, allocator);
        }
    }

    if(need_extract) {
        document.AddMember("image-sections", img_sections, allocator);
        document.AddMember("ttf-sections", ttf_sections, allocator);

        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);
        ofstream config(out_path_pref + "_config.json", ios::out);
        config.write(buffer.GetString(), buffer.GetSize());
    }

    return 0;
}

int packPicturesSection(const string& conf_file_path, const string& out_path_pref)
{
    ifstream config_file(conf_file_path);
    if(config_file.fail()) {
        cerr << "can't open config file: " << conf_file_path << endl;
        return -1;
    }

    ofstream out_file(out_path_pref + "_patched.bin", ios::binary | ios::out);

    string content((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
    Document document;
    document.Parse(content.c_str());

    Value& v = document["header"];
    string header_file_path = v.GetString();
    ifstream header_file(header_file_path, ios::binary | std::ios::ate);
    if(header_file.fail()) {
        cerr << "Can't open header " << header_file_path << endl;
        return -1;
    }
    streamsize header_size = header_file.tellg();
    header_file.seekg(0, ios::beg);

    vector<uint8_t> header_data(header_size);
    if (!header_file.read(reinterpret_cast<char *>(header_data.data()), header_size))
        cerr << "Can't read header file" << endl;

    header_file.close();

    out_file.write(reinterpret_cast<char *>(header_data.data()), header_size);

    string vbf_header((istreambuf_iterator<char>(header_file)), istreambuf_iterator<char>());
    size_t vbf_header_len = vbf_header.length();

    auto packSection = [&out_file, &document](const char* section_name){
        char zero =0;
        const Value& section = document[section_name];
        assert(section.IsArray());

        for (SizeType i = 0; i < section.Size(); ++i) {

            auto sec_obj = section[i].GetObject();
            const Value& file = sec_obj["file"];
            string file_path = file.GetString();
            size_t actual_sz = sec_obj["actual-size"].GetInt();
            size_t real_sz = sec_obj["padded-size"].GetInt();

            if(actual_sz > real_sz) {
                cerr << "actual_sz greater than padded size"<< endl;
                return -1;
            } else if(actual_sz != real_sz) {
                cout << "file will be zero padded to size " << real_sz << "with "
                     << real_sz - actual_sz << " bytes" << endl;
            }

            ifstream img_file(file_path, ios::binary | std::ios::ate);
            if(img_file.fail()) {
                cerr << "Can't open image file " << file_path << endl;
                return -1;
            }
            auto img_size = img_file.tellg();
            img_file.seekg(0, ios::beg);
            if(img_size != actual_sz) {
                cerr << "actual_sz and file (" << file_path << ") size mismatch"<< endl;
                return -1;
            }

            vector<uint8_t> img_data(img_size);
            if (!img_file.read(reinterpret_cast<char *>(img_data.data()), img_size)){
                cerr << "Can't read image file" << file_path << endl;
                return -1;
            }

            img_file.close();

            out_file.write(reinterpret_cast<char *>(img_data.data()), img_size);
            for(int j=actual_sz; j < real_sz; ++j)
                out_file.write(&zero, 1);
        }
        return 0;
    };

    packSection("image-sections");
    packSection("ttf-sections");
    out_file.close();

    return 0;
}

int main(int argc, char **argv)
{
    try {

        bool save = false;
        bool pack = false;

        cxxopts::Options options("imgparcer", "Ford IPC image excructur");
        options.add_options()
                ("u,unpack","Save extracted data to separate file", cxxopts::value<bool>(save))
                ("p,pack","Pack image section", cxxopts::value<bool>(pack))
                ("i,input","Input file", cxxopts::value<string>())
                ("o,output","Output directory", cxxopts::value<string>()->default_value(""))
                ("h,help","Print help");

        options.parse_positional({"input"});
        auto result = options.parse(argc, argv);

        if(result.arguments().empty() || result.count("help")){
            cout << options.help() << std::endl;
            return 0;
        }

        if(!result.count("input")){
            cout << "Please, specify input file" << std::endl;
            return 0;
        }

        if(pack) {
            if(packPicturesSection(result["input"].as<string>(), result["output"].as<string>()))
            {
                return 0;
            }
        } else {
            if(parsePicturesSection(result["input"].as<string>(), result["output"].as<string>(), true, save)) {
                return 0;
            }
        }

    } catch (const cxxopts::OptionException& e){
        cout << "error parsing options: " << e.what() << endl;
    }

    return 0;
}
