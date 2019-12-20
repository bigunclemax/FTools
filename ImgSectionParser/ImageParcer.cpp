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
    uint32_t unk1;
    uint32_t unk2;
    uint8_t  img_type;
    char     fileName[31];
};

struct ttf_file {
    char fileName[32];
};

struct extracted_item {
    uint32_t unk1;
    uint32_t unk2;
    uint8_t  unk3;
    array<char, 32> ascii_string;
    uint32_t size;
    vector<uint8_t> data;
};

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

int
parsePicturesSection(const string& file_path,
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
        str_buff << out_path_pref << "_header.bin";

        SaveToFile(str_buff.str(),
                   reinterpret_cast<char *>(file_buff.data()),read_idx);

        document.AddMember("header", Value(str_buff.str().c_str(), allocator), allocator);
    }

    if(verbose) {
        int g_idx = 1;

        cout
                << "   # "
                << " | "
                << "          ASCII string          "
                << " | "
                << "  Size   "
                << " | "
                << "      Type      "
                << " | "
                << " Unknown #0 "
                << " | "
                << " Unknown #1 "
                << " | "
                << " Note "
                << endl;

        auto zip_header_ptr = reinterpret_cast<struct zip_file *>(zip_headers_offset);
        for(int i=0; i < zip_count; ++i, g_idx++) {

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

            //get relative offset
            auto relative_offset =  _addr - 0x02400000;

            mz_zip_archive zip_archive;
            memset(&zip_archive, 0, sizeof(zip_archive));
            if(!mz_zip_reader_init_mem(&zip_archive, reinterpret_cast<const void *>(&file_buff[relative_offset]), real_sz,  0))
            {
                cerr << "Can't initialize archive";
                return -1;
            }

            char _fileName[32];
            mz_zip_reader_get_filename(&zip_archive, 0, _fileName, 32);
            mz_zip_reader_end(&zip_archive);

//            auto padded_sz = actual_sz + actual_sz % 4;

            cout << resetiosflags(ios::hex)  << std::setfill(' ')
                 << setw(5) << g_idx
                 << " | "
                 << setiosflags(ios::left) << setw(32) << zip_header.fileName << resetiosflags(ios::left)
                 << " | "
                 <<" " << setw(8) << real_sz
                 << " | "
                 << ToString(static_cast<image_type>(zip_header.img_type))
                 << std::setfill('0') << hex << " 0x" << setw(2) << (int)zip_header.img_type << " "
                 << " | "
                 << hex << " 0x" << setw(8) << zip_header.unk1 << " "
                 << " | "
                 << hex << " 0x" << setw(8) << zip_header.unk2 << " "
                 << " | "
                 << _fileName
                 << endl;

            if(need_extract) {

                stringstream str_buff;
                str_buff << out_path_pref << g_idx << ".zip";

                SaveToFile(str_buff.str(),
                           reinterpret_cast<char *>(&file_buff[relative_offset]),real_sz);

                Value section_obj(kObjectType);
                {
                    section_obj.AddMember("file", Value(str_buff.str().c_str(), allocator), allocator);
                    stringstream _addr_hex;
                    _addr_hex << "0x" << std::hex << relative_offset;
                    section_obj.AddMember("relative-offset", Value(_addr_hex.str().c_str(), allocator), allocator);
                    section_obj.AddMember("actual-size", actual_sz, allocator);
                    section_obj.AddMember("size", real_sz, allocator);
                }
                img_sections.PushBack(section_obj, allocator);
            }
        }

        auto ttf_header_ptr = reinterpret_cast<struct ttf_file *>(ttf_headers_offset);
        for(int i=0; i < ttf_count; ++i, g_idx++) {

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

            //get relative offset
            auto relative_offset =  _addr - 0x02400000;

            cout << resetiosflags(ios::hex)  << std::setfill(' ')
                 << setw(5) << g_idx
                 << " | "
                 << setiosflags(ios::left) << setw(32) << ttf_header.fileName << resetiosflags(ios::left)
                 << " | "
                 << hex << "0x" << std::setfill('0') << setw(8) << actual_sz << resetiosflags(ios::hex)
                 <<" (" << setw(8) << real_sz << ')'
                 << endl;

            if(need_extract) {

                stringstream str_buff;
                str_buff << out_path_pref << g_idx << ".ttf";

                SaveToFile(str_buff.str(),
                           reinterpret_cast<char *>(file_buff[relative_offset]), real_sz);


                Value section_obj(kObjectType);
                {
                    section_obj.AddMember("file", Value(str_buff.str().c_str(), allocator), allocator);
                    stringstream _addr_hex;
                    _addr_hex << "0x" << std::hex << relative_offset;
                    section_obj.AddMember("relative-offset", Value(_addr_hex.str().c_str(), allocator), allocator);
                    section_obj.AddMember("actual-size", actual_sz, allocator);
                    section_obj.AddMember("size", real_sz, allocator);
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
    }

    if(need_extract) {
#if 0
        //extract binary content
        auto binary_content_offset = &file_buff[read_idx];
        extracted_items.resize(zipFilesVec.size() + ttfFilesVec.size());

        for(auto i =0; i < zipFilesVec.size() - 1; i++ ){
            auto start_addr = stoul(&zipFilesVec[i].fileName[6], nullptr, 16);
            auto end_addr = stoul(&zipFilesVec[i+1].fileName[6], nullptr, 16);
            auto item_size = end_addr - start_addr;

            extracted_items[i].size = item_size;
            extracted_items[i].unk1 = zipFilesVec[i].unk1;
            extracted_items[i].unk2 = zipFilesVec[i].unk2;
            extracted_items[i].ascii_string = zipFilesVec[i].fileName;
            extracted_items[i].data.resize(item_size);
            copy(file_buff.begin() + read_idx,
                 file_buff.begin() + read_idx + item_size,
                 extracted_items[i].data.begin());
            read_idx += item_size;
        }

        //last ZIP
        auto item_idx = zipFilesVec.size()-1;
        auto start_addr = stoul(&zipFilesVec[item_idx].fileName[6], nullptr, 16);
        auto end_addr = stoul(&ttfFilesVec[0].fileName[5], nullptr, 16);
        auto item_size = end_addr - start_addr;

        extracted_items[item_idx].size = item_size;
        extracted_items[item_idx].unk1 = zipFilesVec[item_idx].unk1;
        extracted_items[item_idx].unk2 = zipFilesVec[item_idx].unk2;
        extracted_items[item_idx].ascii_string = zipFilesVec[item_idx].fileName;
        extracted_items[item_idx].data.resize(item_size);
        copy(file_buff.begin() + read_idx,
             file_buff.begin() + read_idx + item_size,
             extracted_items[item_idx].data.begin());

        read_idx += item_size;

        cout << "Last item size" << item_size << endl;
        cout << "TTF start addt 0x" << hex << read_idx << endl;

        //read first ttf
        for(auto i =0; i < ttfFilesVec.size() - 1; i++ ) {
            start_addr = stoul(&ttfFilesVec[i].fileName[5], nullptr, 16);
            end_addr = stoul(&ttfFilesVec[i+1].fileName[5], nullptr, 16);
            item_size = end_addr - start_addr;

            auto &item = extracted_items[i + zipFilesVec.size()];
            item.size = item_size;
            item.unk1 = 0;
            item.unk2 = 0;
            item.ascii_string = ttfFilesVec[0].fileName;
            item.data.resize(item_size);
            copy(file_buff.begin() + read_idx,
                 file_buff.begin() + read_idx + item_size,
                 item.data.begin());

            read_idx += item_size;
        }

        item_size = file_buff.size() - read_idx;

        auto &last_extracted_item = extracted_items.back();
        last_extracted_item.size = item_size;
        last_extracted_item.unk1 = 0;
        last_extracted_item.unk2 = 0;
        last_extracted_item.ascii_string = ttfFilesVec[1].fileName;
        last_extracted_item.data.resize(item_size);
        copy(file_buff.begin() + read_idx,
             file_buff.begin() + read_idx + item_size,
             last_extracted_item.data.begin());

        cout
                << " # "
                << " | "
                << "          ASCII string          "
                << " | "
                << "        Size         "
                << " | "
                << " Unknown #0 "
                << " | "
                << " Unknown #1 "
                << " | "
                << endl;

        auto j = 1;
        for(auto itm: extracted_items){
            string ascii_str(begin((itm).ascii_string), end((itm).ascii_string));
            cout << resetiosflags(ios::hex)  << std::setfill(' ')
                 << setw(3) << j++
                 << " | "
                 << setiosflags(ios::left) << setw(32) << ascii_str << resetiosflags(ios::left)
                 << " | "
                 << hex << "0x" << std::setfill('0') << setw(8) << (itm).size << resetiosflags(ios::hex)
                 <<" (" << setw(8) << (itm).size << ')'
                 << " | "
                 << hex << " 0x" << setw(8) << (itm).unk1 << " "
                 << " | "
                 << hex << " 0x" << setw(8) << (itm).unk2 << " "
                 << " | "
                 << endl;
        }
#endif
    }

    return 0;
}

int main(int argc, char **argv)
{
    try {

        bool save = false;

        cxxopts::Options options("imgparcer", "Ford IPC image excructur");
        options.add_options()
                ("s,save","Save extracted data to separate file", cxxopts::value<bool>(save))
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

        vector<struct extracted_item> extracted_items;
        if(parsePicturesSection(result["input"].as<string>(), result["output"].as<string>(), true, save)) {
            return 0;
        }
#if 0
        if(save) {

            auto SaveToFile = [](string file_name, char *data_ptr, int data_len) {
                ofstream out_file(file_name, ios::out | ios::binary);
                if (!out_file) {
                    cout << "file: " << file_name << " can't be created" << endl;
                } else {
                    out_file.write(data_ptr, data_len);
                }
                out_file.close();
            };

            for (auto item = extracted_items.begin(); item != extracted_items.end(); item++)
            {
                stringstream str_buff;
                str_buff
                    << result["output"].as<string>()
                    << distance(extracted_items.begin(), item)
                    << (((*item).ascii_string[0] != '~' )? ".zip" : ".ttf");

                SaveToFile(str_buff.str(),
                        reinterpret_cast<char *>(&(*item).data[0]),
                        (*item).data.size());
            }
        }
#endif
    } catch (const cxxopts::OptionException& e){
        cout << "error parsing options: " << e.what() << endl;
    }

    return 0;
}
