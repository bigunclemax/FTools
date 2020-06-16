//
// Created by user on 03.05.2020.
//
#include <filesystem>
#include <VbfFile.h>
#include <ImageSection.h>
#include <miniz_zip.h>
#include <utils.h>
#include <EifConverter.h>
#include <cxxopts.hpp>
#include <csv.h>
#include <CRC.h>


static const char* ITEM_IDX = "Idx";
static const char* ITEM_NAME = "Name";
static const char* ITEM_WIDTH = "Width";
static const char* ITEM_HEIGHT = "Height";
static const char* ITEM_TYPE = "Depth";
static const char* ITEM_PALETTE_CRC = "palette_crc16";
static const char* CUSTOM_DIR = "custom";

namespace fs = std::filesystem;

struct Opts {
    fs::path conf_p;
    fs::path out_p;
    fs::path vbf_p;
    bool pack;
};

void parse_opts(int argc, char **argv, Opts& opts) {

    try {
        cxxopts::Options options("imgunpkr", "Ford IMG resources unpacker");
        options.add_options()
                ("p,pack","Pack VBF file")
                ("u,unpack","Unpack VBF file")
                ("c,conf","Config file", cxxopts::value<string>())
                ("o,output","Output path", cxxopts::value<string>())
                ("v,vbf","VBF file which will be patched", cxxopts::value<string>())
                ("h,help","Print help");

        options.parse_positional({"vbf"});
        auto result = options.parse(argc, argv);

        if(result.arguments().empty() || result.count("help")){
            cout << options.help() << std::endl;
            exit(0);
        }

        if(result.count("pack")) {
            opts.pack = true;

            if(!result.count("conf")){
                cout << "Please, specify VBF file" << std::endl;
                exit(0);
            }

            opts.conf_p = result["conf"].as<string>();
            if(!fs::exists(opts.conf_p)) {
                cout << "VBF file not found" << std::endl;
                exit(0);
            }

        } else if (!result.count("unpack")) {
            cout << "Please, specify mode (pack|unpack)" << std::endl;
            exit(0);
        }

        if(!result.count("vbf")){
            cout << "Please, specify vbf file" << std::endl;
            exit(0);
        }

        fs::path in_file(result["vbf"].as<string>());
        if(!fs::exists(in_file)) {
            cout << "Input file not found" << std::endl;
            exit(0);
        }

        fs::path out_path;
        if(result.count("output")){
            out_path = result["output"].as<string>();
            if(!fs::exists(out_path) || !fs::is_directory(out_path)) {
                cout << "Output dir not found" << std::endl;
                exit(0);
            }
        } else {
            out_path = in_file.parent_path();
        }

        opts.vbf_p = in_file;
        opts.out_p = out_path;

    } catch (const cxxopts::OptionException& e){
        cout << "error parsing options: " << e.what() << endl;
        exit(-1);
    }
}

vector<uint8_t> GetEIFfromImgSection(ImageSection& img_sec, int idx, std::string* p_eif_name = nullptr) {

    std::vector<uint8_t> img_zip_bin;
    img_sec.GetItemData(ImageSection::RT_ZIP, idx, img_zip_bin);

    // unzip eif
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_mem(&zip_archive,
    (const void *)img_zip_bin.data(), img_zip_bin.size(), 0))
    {
        throw runtime_error("Can't get image name form archive");
    }
    mz_zip_archive_file_stat file_stat;
    mz_zip_reader_file_stat(&zip_archive, 0 , &file_stat);
    vector<uint8_t> eif(file_stat.m_uncomp_size);
    mz_zip_reader_extract_to_mem(&zip_archive, 0, (void *)eif.data(), eif.size(), 0);
    mz_zip_reader_end(&zip_archive);

    if(nullptr != p_eif_name) {
        *p_eif_name = file_stat.m_filename;
    }

    return eif;
}

int UnpackImg(const fs::path& in_path, const fs::path& out_path) {

    //unpack vbf
    VbfFile vbf;
    vbf.OpenFile(in_path.string());
    if(!vbf.IsOpen()) {
        throw runtime_error("Can't parse vbf");
    }

    std::vector<uint8_t> img_sec_bin;
    if(vbf.GetSectionRaw(1, img_sec_bin)) {
        throw runtime_error("Can't get image section");
    }

    //unpack image section
    ImageSection img_sec;
    img_sec.Parse(img_sec_bin);

    //unpack eifs
    std::ofstream export_list (out_path / "export_list.csv");
    export_list << ITEM_IDX << ","
                << ITEM_NAME << ","
                << ITEM_TYPE << ","
                << ITEM_PALETTE_CRC << ","
                << ITEM_WIDTH << ","
                << ITEM_HEIGHT
                << std::endl;

    int zip_items = img_sec.GetItemsCount(ImageSection::RT_ZIP);

    fs::path eifs_path = out_path/"eif";
    fs::path bmps_path = out_path/"bmp";

    fs::create_directory(eifs_path);
    fs::create_directory(bmps_path);
    fs::create_directory(out_path/CUSTOM_DIR);

    for(int i = 0; i < zip_items; i++) {

        std::string eif_name;
        auto eif = GetEIFfromImgSection(img_sec, i, &eif_name);

        FTUtils::bufferToFile((eifs_path/eif_name).string(), (char*)eif.data(), eif.size());

        //convert eif to bmp
        auto p = bmps_path/eif_name;
        p.replace_extension(".bmp");
        EIF::EifConverter::eifToBmpFile(eif, p.string());

        //fill csv row
        string crc = "0";
        auto header_p = reinterpret_cast<EIF::EifBaseHeader*>(eif.data());
        if(header_p->type == EIF_TYPE_MULTICOLOR) {
            auto crc16 = CRC::Calculate((char*)eif.data()+0x10, 768, CRC::CRC_16_CCITTFALSE());
            stringstream pal_crc;
            pal_crc << hex << setw(4) << setfill('0') << uppercase << crc16;
            crc = pal_crc.str();
        }

        export_list << i << ","
            << eif_name << ","
            << ToColorDepth((image_type)header_p->type) << ","
            << crc  << ","
            << header_p->width << ","
            << header_p->height
            << std::endl;
    }

    fs::path ttf_path = out_path/"ttf";
    fs::create_directory(ttf_path);
    int ttf_items = img_sec.GetItemsCount(ImageSection::RT_TTF);
    for(int i = 0; i < ttf_items; i++) {
        string ttf_name = std::to_string(i) + ".ttf";
        std::vector<uint8_t> item_bin;
        img_sec.GetItemData(ImageSection::RT_TTF, i, item_bin);
        FTUtils::bufferToFile((ttf_path/ttf_name).string(), (char*)item_bin.data(), item_bin.size());
        export_list << i << ","
                    << ttf_name << ","
                    << 0 << ","
                    << 0 << ","  << 0 << "," << 0 << std::endl;
    }

    export_list.close();

    return 0;
}

int compressVector(const std::vector<uint8_t>& data, const char * data_name, std::vector<uint8_t>& compressed_data) {

    mz_bool status;
    mz_zip_archive zip_archive = {};
    unsigned flags = MZ_DEFAULT_LEVEL | MZ_ZIP_FLAG_ASCII_FILENAME;

    status = mz_zip_writer_init_heap(&zip_archive, 0, 0);
    if (!status) {
        cerr << "mz_zip_writer_init_heap failed!";
        return -1;
    }

    status = mz_zip_writer_add_mem_ex(&zip_archive, data_name, (void*)data.data(), data.size(), "", 0, flags, 0, 0);
    if (!status) {
        cerr << "mz_zip_writer_add_mem_ex failed!";
        return -1;
    }

    std::size_t size;
    void *pBuf;
    status = mz_zip_writer_finalize_heap_archive(&zip_archive, &pBuf, &size);
    if (!status) {
        cerr << "mz_zip_writer_finalize_heap_archive failed!";
        return -1;
    }

    //copy compressed data to vector
    compressed_data.resize(size);
    copy((uint8_t*)pBuf, (uint8_t*)pBuf + size, compressed_data.begin());

    status = mz_zip_writer_end(&zip_archive);
    if (!status) {
        cerr << "mz_zip_writer_end failed!";
        return -1;
    }

    return 0;
}

struct csv_row {
    uint32_t idx, type, crc;
    std::string name;
};

csv_row GetResCsvData(const std::vector<csv_row>& csv, const std::string& res_name) {

    csv_row csv_data;
    for (auto& csv_row :csv) {
        if(csv_row.name == res_name)
            csv_data = csv_row;
    }
    return csv_data;
}
std::string GetNameFromIdx(const std::vector<csv_row>& csv, int idx) {

    for (auto& csv_row :csv) {
        if(csv_row.idx == idx && fs::path(csv_row.name).extension() == ".eif")
            return csv_row.name;
    }
    return "";
}

vector<uint32_t> GetResWithSamePalette(const std::vector<csv_row>& csv, const uint32_t crc) {
    vector<uint32_t> indexes;
    for (auto& csv_row :csv) {
        if(csv_row.crc == crc)
            indexes.push_back(csv_row.idx);
    }
    return indexes;
}

void CompressAndReplaceEIF(ImageSection& img_sec, int idx, const vector<uint8_t>& res_bin, const std::string& res_name) {

    //get header data
    auto eif_header_p = reinterpret_cast<const EIF::EifBaseHeader*>(res_bin.data());

    //compress
    vector<uint8_t> zip_bin;
    compressVector(res_bin, res_name.c_str(), zip_bin);

    //replace
    img_sec.ReplaceItem(ImageSection::RT_ZIP, idx, zip_bin,
    eif_header_p->width, eif_header_p->height, eif_header_p->type);
    //TODO: add msg
}

int RepackResources(const fs::path& config_path, ImageSection& img_sec, const std::vector<csv_row>& csv) {

    fs::path custom_dir = config_path.parent_path()/CUSTOM_DIR;

    std::map <int, std::map<int, EIF::EifImage16bit>> eif16_map;

    // for each resources
    for (const auto & entry : fs::directory_iterator(custom_dir)) {

        auto& res_path = entry.path();
        auto res_name = res_path.filename();

        //find in csv
        auto res_csv_data = GetResCsvData(csv, res_name); //TODO: replace bmp to eif
        if(res_csv_data.name.empty()) {
            cerr << "Unknown resource " << res_name << endl;
            return -1;
        }

        if(res_path.extension() == "ttf") {
            //TODO: add msg
            vector<uint8_t> res_bin;
            FTUtils::fileToVector(res_path, res_bin);
            img_sec.ReplaceItem(ImageSection::RT_TTF, res_csv_data.idx, res_bin);
        }

        if(res_path.extension() == "bmp") {

            if(res_csv_data.type == 16) {

                vector<uint8_t> res_bin;
                EIF::EifImage16bit eif;
                eif.openBmp(res_path);
                eif16_map[res_csv_data.crc][res_csv_data.idx] = eif;

            } else {

                // create eif from BMP and save it to vector
                EIF::EifImageBase* eif;

                if(res_csv_data.type == 8) {
                    eif = new EIF::EifImage8bit();
                } else if(res_csv_data.type == 32) {
                    eif = new EIF::EifImage32bit();
                } else {
                    cerr << "Unknown resource type" << res_name << endl;
                    return -1;
                }

                vector<uint8_t> res_bin;
                eif->openBmp(res_path);
                eif->saveEifToVector(res_bin);
                delete eif;

                CompressAndReplaceEIF(img_sec, res_csv_data.idx, res_bin, res_name);
            }
        }

        if(res_path.extension() == "eif") {
            //TODO:
        }
    }

    for(auto& it : eif16_map) {

        std::vector<EIF::EifImage16bit*> eifs;

        auto indexes = GetResWithSamePalette(csv, it.first);
        for (auto& idx : indexes) {
            if ( it.second.find(idx) == it.second.end() ) {
                //TODO: msg eif getting from vbf
                auto eif_bin = GetEIFfromImgSection(img_sec, idx);
                EIF::EifImage16bit eif;
                eif.openEif(eif_bin);
                it.second[idx] = eif;
            }
            eifs.push_back(&it.second[idx]);
        }

        //calc multi palette
        EIF::EifConverter::createMultipaletteEifs(eifs);

        //pack
        for(auto& itt : it.second) {
            std::string eif_name = GetNameFromIdx(csv, itt.first);
            vector<uint8_t> res_bin;
            itt.second.saveEifToVector(res_bin);
            CompressAndReplaceEIF(img_sec, itt.first, res_bin, eif_name);
        }
    }

    return 0;
}

//eif init from raw
//eif getBitmap


std::vector<csv_row> ReadCSV(const fs::path& config_path) {

    std::vector<csv_row> vec;
    vec.reserve(1310);

    io::CSVReader<4> in(config_path.string());
    in.read_header(io::ignore_extra_column, ITEM_IDX, ITEM_NAME, ITEM_TYPE, ITEM_PALETTE_CRC);

    std::string name, crc;
    uint32_t idx, type;

    while(in.read_row(idx, name, type, crc)) {
        vec.push_back({idx,type, (uint32_t)std::stoi(crc, nullptr, 16) ,name});
    }

    return vec;
}

int PackImg(const fs::path& config_path, const fs::path& vbf_path, const fs::path& out_path) {

    //open vbf and get image section
    VbfFile vbf;
    vbf.OpenFile(vbf_path.string());
    if(!vbf.IsOpen()) {
        throw runtime_error("Can't open vbf");
    }

    std::vector<uint8_t> img_sec_bin;
    if(vbf.GetSectionRaw(1, img_sec_bin)) {
        throw runtime_error("Can't get image section");
    }

    ImageSection img_sec;
    img_sec.Parse(img_sec_bin);

    auto csv = ReadCSV(config_path);

    //open custom dif
    RepackResources(config_path, img_sec, csv);

    img_sec.SaveToVector(img_sec_bin);

    //replace vbf image content
    vbf.ReplaceSectionRaw(1, img_sec_bin);

    //pack vbf
    vbf.SaveToFile((out_path/"patched.vbf").string());

    return 0;
}

int main(int argc, char **argv) {

    Opts opts = {};
    parse_opts(argc, argv, opts);

    if(opts.pack) {
        PackImg(opts.conf_p, opts.vbf_p, opts.out_p);
    } else {
        UnpackImg(opts.vbf_p, opts.out_p);
    }

    return 0;
}