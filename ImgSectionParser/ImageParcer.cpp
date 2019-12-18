//
// Created by bigun on 12/20/2018.
//

#include <iostream>
#include <fstream>
#include <streambuf>
#include <list>
#include <regex>
#include <iomanip>


#include "CRC.h"
#include <cxxopts.hpp>

using namespace std;


#define START_ZIP_SECTION 0x9094


struct zip_file {
    uint32_t unk1;
    uint32_t unk2;
    array<char, 32> fileName;
};

struct ttf_file {
    array<char, 32> fileName;
};

struct extracted_item {
    uint32_t unk1;
    uint32_t unk2;
    array<char, 32> ascii_string;
    uint32_t size;
    vector<uint8_t> data;
};

int
parsePicturesSection(const string& file_path, vector<struct extracted_item>& extracted_items)
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

    //read Zip items count
    auto read_idx = START_ZIP_SECTION;
    uint32_t itemsCount = *reinterpret_cast<uint32_t *>(&file_buff[read_idx]);
    read_idx += sizeof(uint32_t);
    cout << "ZIP Items count " << itemsCount << endl;
    auto zipFilesVec = vector<struct zip_file>(itemsCount);
    for(auto i=0; i < itemsCount; i++) {
        zipFilesVec[i] = reinterpret_cast<struct zip_file *>(&file_buff[read_idx])[i];
    }
    read_idx += sizeof(struct zip_file) * zipFilesVec.size();

    //read TTF items count
    itemsCount = *reinterpret_cast<uint32_t *>(&file_buff[read_idx]);
    read_idx += sizeof(uint32_t);
    cout << "TTF Items count " << itemsCount << endl;
    auto ttfFilesVec = vector<struct ttf_file>(itemsCount);
    for(auto i=0; i < itemsCount; i++) {
        ttfFilesVec[i] = reinterpret_cast<struct ttf_file *>(&file_buff[read_idx])[i];
    }
    read_idx += sizeof(struct ttf_file) * ttfFilesVec.size();

    auto magicInt =  *reinterpret_cast<uint32_t *>(&file_buff[read_idx]);
    cout << "yoba magic num 0x" << hex << magicInt << resetiosflags(ios::hex) << endl;
    read_idx += sizeof(uint32_t);
    cout << "data offset is 0x" << hex << read_idx << resetiosflags(ios::hex) << endl;

    //read binary content
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
        if(parsePicturesSection(result["input"].as<string>(), extracted_items)) {
            return 0;
        }

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
    } catch (const cxxopts::OptionException& e){
        cout << "error parsing options: " << e.what() << endl;
    }

    return 0;
}
