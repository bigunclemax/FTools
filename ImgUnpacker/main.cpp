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
                ("o,output","Output file", cxxopts::value<string>())
                ("v,vbf","VBF file which will be patched", cxxopts::value<string>())
                ("h,help","Print help");

//        options.parse_positional({"vbf"});
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

int UnpackImg(const fs::path& in_path, const fs::path& out_path) {

    //unpack vbf
    VbfFile vbf;
    vbf.OpenFile(in_path.string());
    if(!vbf.IsOpen()) {
        return -1; //can't parse vbf
    }

    std::vector<uint8_t> img_sec_bin;
    if(vbf.GetSectionRaw(1, img_sec_bin)) {
        return -1; //can't get image function
    }

    //unpack image section
    ImageSection img_sec;
    img_sec.Parse(img_sec_bin);

    img_sec.Export(out_path);

    //unpack eifs
    int zip_items = img_sec.GetItemsCount(ImageSection::RT_ZIP);

    fs::path eifs_path = out_path/"eif";
    fs::path bmps_path = out_path/"bmp";

    fs::create_directory(eifs_path);
    fs::create_directory(bmps_path);

    for(int i = 0; i < zip_items; i++) {
        std::vector<uint8_t> img_zip_bin;
        img_sec.GetItemData(ImageSection::RT_ZIP, i, img_zip_bin);

        // unzip eif
        mz_zip_archive zip_archive;
        memset(&zip_archive, 0, sizeof(zip_archive));
        if (!mz_zip_reader_init_mem(&zip_archive,
                                    (const void *)img_zip_bin.data(), img_zip_bin.size(), 0))
        {
            throw runtime_error("Can't get image name form archive");
        }
        unsigned file_index =0;
        mz_zip_archive_file_stat file_stat;
        mz_zip_reader_file_stat(&zip_archive, file_index , &file_stat);
        vector<uint8_t> eif(file_stat.m_uncomp_size);
        mz_zip_reader_extract_to_mem(&zip_archive, file_index, (void *)eif.data(), eif.size(), 0);
        mz_zip_reader_end(&zip_archive);

        FTUtils::bufferToFile((eifs_path/file_stat.m_filename).string(), (char*)eif.data(), eif.size());

        //convert eif to bmp
        auto p = bmps_path/file_stat.m_filename;
        p.replace_extension(".bmp");
        EIF::EifConverter::eifToBmpFile(eif, p.string());

    }
    return 0;
}

int PackImg(const fs::path& config_path, const fs::path& vbf_path, const fs::path& out_path) {

    //open image section config
    //pack image section
    ImageSection section;
    section.Import(config_path);

    vector<uint8_t> v;
    fs::path tmp_f = out_path/"ft.tmp";
    section.SaveToFile(tmp_f);
    FTUtils::fileToVector(tmp_f, v);
    fs::remove(tmp_f);

    //open vbf
    VbfFile vbf;
    vbf.OpenFile(vbf_path.string());
    if(!vbf.IsOpen()) {
        throw runtime_error("Can't open vbf");
    }

    //replace vbf image content
    vbf.ReplaceSectionRaw(1, v);

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