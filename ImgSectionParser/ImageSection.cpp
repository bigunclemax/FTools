//
// Created by user on 04.05.2020.
//
#include <algorithm>
#include <regex>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <fstream>
#include <utils.h>
#include <sstream>

#include "ImageSection.h"

using namespace rapidjson;

void ImageSection::GetItemData(const vector<uint8_t>& bin_data, const char* fileName, vector<uint8_t>& out) {

    // get actual size and offset
    smatch m;
    string _str(fileName);
    regex_search(_str, m, regex("(\\d+)\\."));
    if (m.size() != 2) {
        throw runtime_error("header name not contain actual size!");
    }
    auto actual_sz = stoul(m[1].str(), nullptr, 10);
    auto _addr = stoul(&fileName[5], nullptr, 16);

    //get relative offset
    auto relative_offset =  _addr - 0x02400000;

    out.resize(actual_sz);
    copy(bin_data.begin() + relative_offset,
            bin_data.begin() + relative_offset + actual_sz,
            out.begin());
}

void ImageSection::Parse(const vector<uint8_t> &bin_data) {

    auto read_idx = FindSectionData(bin_data);

    // copy header
    m_header_data.resize(read_idx);
    copy(bin_data.begin(), bin_data.begin() + read_idx, m_header_data.begin());

    // parse Zip headers block
    uint32_t _zip_count = *reinterpret_cast<const uint32_t *>(&bin_data[read_idx]);
    read_idx += sizeof(uint32_t);
    auto zip_headers_offset = &bin_data[read_idx];
    read_idx += sizeof(struct zip_file) * _zip_count;

    // parse TTF headers block
    uint32_t _ttf_count = *reinterpret_cast<const uint32_t *>(&bin_data[read_idx]);
    read_idx += sizeof(uint32_t);
    auto ttf_headers_offset = &bin_data[read_idx];
    read_idx += sizeof(struct ttf_file) * _ttf_count;

    m_unknownInt =  *reinterpret_cast<const uint32_t *>(&bin_data[read_idx]);

    // extract zip resources
    m_zip_vec.resize(_zip_count);
    auto zip_header_ptr = reinterpret_cast<const struct zip_file *>(zip_headers_offset);
    for(int i=0; i < _zip_count; ++i) {
        auto& item = m_zip_vec[i];
        item.res_type = RT_ZIP;
        item.width = zip_header_ptr[i].width;
        item.height = zip_header_ptr[i].height;
        item.img_type = zip_header_ptr[i].img_type;
        GetItemData(bin_data, zip_header_ptr[i].fileName, item.data);
    }

    // extract ttf resources
    m_ttf_vec.resize(_ttf_count);
    auto ttf_header_ptr = reinterpret_cast<const struct ttf_file *>(ttf_headers_offset);
    for(int i=0; i < _ttf_count; ++i) {
        auto& item = m_ttf_vec[i];
        item.res_type = RT_TTF;
        GetItemData(bin_data, ttf_header_ptr[i].fileName, item.data);
    }
}

uint32_t ImageSection::FindSectionData(const vector<uint8_t> &bin_data) {

    vector<uint8_t> v2 = {'~','m','e','m'};
    auto res = search(begin(bin_data), end(bin_data), begin(v2), end(v2));
    if(res == end(bin_data)) {
        throw runtime_error("image or ttf resources not found");
    }
    return res - bin_data.begin() - 13; // 4b - width, 4b - height, 1b -type, 4b - total items;
}


int ImageSection::Export(const fs::path &out_path, const string& name_prefix) {

    Document document;
    document.SetObject();
    Document::AllocatorType& allocator = document.GetAllocator();
    Value img_sections(kArrayType);
    Value ttf_sections(kArrayType);

    fs::create_directory(out_path / "zip");
    fs::create_directory(out_path / "ttf");

    // save header
    auto out_file_str = (name_prefix + "_header.bin");
    FTUtils::bufferToFile((out_path / out_file_str).string(),
                (char *) m_header_data.data(), m_header_data.size());
    document.AddMember("header", Value(out_file_str.c_str(), allocator), allocator);
    document.AddMember("unknown-int", Value().SetUint64(m_unknownInt), allocator);

    auto SaveResources = [&document, &allocator, &out_path](const string& item_prefix, const vector<Item>& items_vector, Value& json_section){
        int i = 0;
        for(auto& item : items_vector) {

            stringstream str_buff;
            str_buff << i++ << "." << item_prefix;
            string file_name = str_buff.str();

            FTUtils::bufferToFile((out_path / item_prefix / file_name).string(),
                        (char *) item.data.data(), item.data.size());

            Value section_obj(kObjectType);
            {
                section_obj.AddMember("file", Value((item_prefix + "/" += file_name).c_str(), allocator), allocator);
                if(RT_ZIP == item.res_type) {
                    section_obj.AddMember("width", item.width, allocator);
                    section_obj.AddMember("height", item.height, allocator);
                    section_obj.AddMember("type", Value(ToString(static_cast<image_type>(item.img_type)), allocator), allocator);
//                  section_obj.AddMember("content", Value(_fileName, allocator), allocator); //TODO: add zip name
                }
            }
            json_section.PushBack(section_obj, allocator);
        }
    };

    SaveResources("zip", m_zip_vec, img_sections);
    SaveResources("ttf", m_ttf_vec, ttf_sections);

    // save config JSON
    document.AddMember("image-sections", img_sections, allocator);
    document.AddMember("ttf-sections", ttf_sections, allocator);

    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);
    ofstream config(out_path / (name_prefix + "_config.json"), ios::out);
    config.write(buffer.GetString(), buffer.GetSize());

    return 0;
}

int ImageSection::GetItemsCount(ImageSection::enResType res_type) {

    if(RT_ZIP == res_type) {
        return m_zip_vec.size();
    }

    if(RT_TTF == res_type) {
        return m_ttf_vec.size();
    }

    return 0;
}

int ImageSection::Import(const fs::path &config_path) {

//    fs::path _config_path(fs::absolute(config_path));
    fs::path config_dir = config_path.parent_path();

    ifstream config_file(config_path);
    if(config_file.fail()) {
        throw runtime_error("file " + config_path.string() + " can't be created");
    }

    // open json and check if all necessary fields exists
    string json_content((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
    Document document;
    if(document.Parse(json_content.c_str()).HasParseError()) {
        throw runtime_error("file " + config_path.string() + " is not a valid JSON");
    }

    if(!document.HasMember("header")
    || !document.HasMember("image-sections")
    || !document.HasMember("ttf-sections")
    || !document.HasMember("unknown-int"))
    {
        throw runtime_error("Config JSON doesn't contain all necessary sections");
    }

    m_unknownInt = document["unknown-int"].GetInt();

    // read header
    Value& v = document["header"];
    string header_path = config_dir.string() + "/" + v.GetString();
    ifstream header_file(header_path, ios::binary | std::ios::ate);
    if(header_file.fail()) {
        throw runtime_error("Can't open header " + config_path.string());
    }
    streamsize header_size = header_file.tellg();
    header_file.seekg(0, ios::beg);

    m_header_data.resize(header_size);
    if (!header_file.read((char *)m_header_data.data(), header_size)) {
        throw runtime_error("Can't read header file " + config_path.string());
    }
    header_file.close();

    auto GetSectionFiles = [&document, &config_dir](const char* section_name, std::vector<Item>& items_vector) {

        const Value& section = document[section_name];
        assert(section.IsArray());
        auto sec_sz = section.Size();

        items_vector.resize(sec_sz);

        for (int i = 0; i < sec_sz; ++i) {

            auto sec_obj = section[i].GetObject();
            const Value& file = sec_obj["file"];
            fs::path file_path =  config_dir / file.GetString();

            if(sec_obj.HasMember("width")) {
                items_vector[i].width = (uint32_t)sec_obj["width"].GetInt();
                items_vector[i].height = (uint32_t)sec_obj["height"].GetInt();
                items_vector[i].img_type = FromString(sec_obj["type"].GetString());
                items_vector[i].res_type = RT_ZIP;
            }

            FTUtils::fileToVector(file_path, items_vector[i].data);
        }
    };

    GetSectionFiles("image-sections", m_zip_vec);
    GetSectionFiles("ttf-sections", m_ttf_vec);

    return 0;
}

int ImageSection::SaveToVector(vector<uint8_t> &v) {

    //write header
    v.resize(m_header_data.size());
    copy(m_header_data.data(), m_header_data.data() + m_header_data.size(), v.begin());

    //calc initial data offset
    uint32_t zip_count = m_zip_vec.size();
    uint32_t ttf_count = m_ttf_vec.size();
    unsigned data_offset = 0x02400000 + m_header_data.size();
    data_offset += zip_count * sizeof(zip_file) + sizeof(uint32_t);
    data_offset += ttf_count * sizeof(ttf_file) + sizeof(uint32_t);
    data_offset += sizeof(uint32_t); // + magic int

    auto createItemsHeaders = [&v, &data_offset](const vector<Item>& items_vector) {

        for (const auto& item : items_vector) {

            uint32_t actual_sz = item.data.size();
            auto remainder = actual_sz % 4;
            uint32_t padded_sz = actual_sz + ((remainder) ? (4 - remainder) : 0);

            //create header
            if(RT_ZIP == item.res_type) {
                zip_file item_h = {};
                item_h.img_type = item.img_type;
                item_h.height = item.height;
                item_h.width = item.width;
                sprintf(item_h.fileName, "~mem/0x%08X-%10d.zip", data_offset, actual_sz);

                copy((char *)&item_h,(char *)&item_h + sizeof(item_h), back_inserter(v));
            } else {
                ttf_file item_h = {};
                sprintf(item_h.fileName, "~mem/0x%08X-%10d.ttf", data_offset, actual_sz);

                copy((char *)&item_h,(char *)&item_h + sizeof(item_h), back_inserter(v));
            }

            data_offset += padded_sz;
        }
    };
    auto packItems = [&v, &data_offset](const vector<Item>& items_vector) {

        for (const auto& item : items_vector) {

            auto actual_sz = item.data.size();
            auto remainder = actual_sz % 4;
            uint32_t padded_sz = actual_sz + ((remainder) ? (4 - remainder) : 0);
            copy((char *)item.data.data(),(char *)item.data.data() + actual_sz, back_inserter(v));

            for(int j = actual_sz; j < padded_sz; ++j)
                v.push_back(0);
        }
    };

    //create zip headers
    copy((char *)&zip_count,(char *)&zip_count + sizeof(uint32_t), back_inserter(v));
    createItemsHeaders(m_zip_vec);

    //create ttf header
    copy((char *)&ttf_count,(char *)&ttf_count + sizeof(uint32_t), back_inserter(v));
    createItemsHeaders(m_ttf_vec);

    //magic int
    copy((char *)&m_unknownInt,(char *)&m_unknownInt + sizeof(uint32_t), back_inserter(v));

    //zip data
    packItems(m_zip_vec);

    //ttf data
    packItems(m_ttf_vec);

    return 0;
}

int ImageSection::SaveToFile(const fs::path &out_path) {

    vector<uint8_t> v;
    SaveToVector(v);
    FTUtils::bufferToFile(out_path.u8string(), (char*)v.data(), v.size());

    return 0;
}

void ImageSection::ParseFile(const fs::path &file) {

    vector<uint8_t> file_data;
    FTUtils::fileToVector(file, file_data);
    Parse(file_data);
}

int ImageSection::GetItemData(ImageSection::enResType res_type, unsigned idx, vector<uint8_t>& bin_data) {

    vector<Item>* vec_ptr;
    if(res_type == RT_ZIP) {
        vec_ptr = &m_zip_vec;
    } else if (res_type == RT_TTF) {
        vec_ptr = &m_ttf_vec;
    } else {
        return -1;
    }

    if(idx >= vec_ptr->size()) {
        return -1;
    }

    bin_data = vec_ptr->at(idx).data;

    return 0;
}

int ImageSection::ReplaceItem(enResType res_type, unsigned idx, const vector<uint8_t>& bin_data,
                              uint32_t w, uint32_t h, uint8_t t)
{

    if (res_type == RT_ZIP) {
        auto& item = m_zip_vec[idx];
        item.data = bin_data;
        item.height = w;
        item.width = h;
        item.img_type = t;
    } else if (res_type == RT_TTF) {
        m_ttf_vec[idx].data = bin_data;
    }

    return 0;
}
