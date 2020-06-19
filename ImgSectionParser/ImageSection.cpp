//
// Created by user on 04.05.2020.
//
#include <algorithm>
#include <regex>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <utils.h>
#include <sstream>
#include <csv.h>

#include "ImageSection.h"

static const char* H_WIDTH = "Width";
static const char* H_HEIGHT = "Height";
static const char* H_X = "Y";
static const char* H_Y = "X";
static const char* H_TYPE = "Type";
static const char* H_Z = "Z-index";
static const char* H_U0 = "Unk0";
static const char* H_U1 = "R";
static const char* H_U2 = "G";
static const char* H_U3 = "B";
static const char* H_U4 = "A";
static const char* H_U5 = "Unk5";

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

    // copy header
    auto records_count = *(uint32_t*)bin_data.data();
    auto read_idx = sizeof(uint32_t);
    m_header_data.resize(records_count);
    m_header_data.assign((HeaderRecord*)&bin_data[read_idx], (HeaderRecord*)&bin_data[read_idx] + records_count);

    read_idx += sizeof(HeaderRecord) * records_count;
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
void ImageSection::HeaderToCsv(const string& csv_file_path) {
    std::ofstream export_list (csv_file_path);
    export_list << H_WIDTH << ","
            << H_HEIGHT << ","
            << H_X << ","
            << H_Y << ","
            << H_TYPE << ","
            << H_Z << ","
            << H_U0 << ","
            << H_U1 << ","
            << H_U2 << ","
            << H_U3 << ","
            << H_U4 << endl;

    for(const auto& hr : m_header_data) {
        export_list << hr.width << ","
                << hr.height << ","
                << hr.X << ","
                << hr.Y << ","
                << (int)hr.type << ","
                << (int)hr.Z << ","
                << (int)hr.unk0 << ","
                << (int)hr.R << ","
                << (int)hr.G << ","
                << (int)hr.B << ","
                << (int)hr.A << endl;
    }
    export_list.close();
}

void ImageSection::HeaderFromCsv(const string &csv_file_path) {

    m_header_data.clear();
    m_header_data.reserve(2000);

    io::CSVReader<11> in(csv_file_path);
    in.read_header(io::ignore_extra_column,
                   H_WIDTH ,
                   H_HEIGHT ,
                   H_X ,
                   H_Y ,
                   H_TYPE ,
                   H_Z ,
                   H_U0 ,
                   H_U1 ,
                   H_U2 ,
                   H_U3 ,
                   H_U4  );

    HeaderRecord hr = {};

    while(in.read_row(hr.width,
                hr.height ,
                hr.X ,
                hr.Y ,
                hr.type ,
                hr.Z ,
                hr.unk0 ,
                hr.R ,
                hr.G ,
                hr.B ,
                hr.A ))
    {
        m_header_data.push_back(hr);
    }
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
    auto out_file_str = (name_prefix + "_header.csv");
    HeaderToCsv(out_file_str);
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
    HeaderFromCsv(header_path);
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
    uint32_t records_count = m_header_data.size();
    auto head_in_bytes = m_header_data.size()*sizeof(HeaderRecord);
    v.resize(head_in_bytes + sizeof(uint32_t));
    copy((char *)&records_count,(char *)&records_count + sizeof(uint32_t), v.begin());
    copy((uint8_t *)m_header_data.data(),(uint8_t *)m_header_data.data() + head_in_bytes,
            v.begin() + sizeof(uint32_t));

    //calc initial data offset
    uint32_t zip_count = m_zip_vec.size();
    uint32_t ttf_count = m_ttf_vec.size();
    unsigned data_offset = 0x02400000 + head_in_bytes + sizeof(uint32_t);
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
        item.width = w;
        item.height = h;
        item.img_type = t;
    } else if (res_type == RT_TTF) {
        m_ttf_vec[idx].data = bin_data;
    }

    return 0;
}
