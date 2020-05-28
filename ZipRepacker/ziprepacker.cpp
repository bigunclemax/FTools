#include <iostream>
#include <fstream>
#include <vector>
#include <cxxopts.hpp>
#include <miniz_zip.h>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;

int repack_zip(const fs::path& input_zip, const fs::path& content_file, const fs::path& out_zip);

int main(int argc, char **argv)
{
    try {

        cxxopts::Options options("ZipRepacker", "Replace content in zip file");
        options.add_options()
                ("i,input","Input zip file", cxxopts::value<string>())
                ("c,content","Content eif file", cxxopts::value<string>())
                ("o,output","Output zip file", cxxopts::value<string>())
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
        auto input_file_name = result["input"].as<string>();

        if(!result.count("content")){
            cout << "Please, specify content file" << std::endl;
            return 0;
        }
        auto conent_file_name = result["content"].as<string>();

        string out_file_name;
        if(result.count("output")){
            out_file_name = result["output"].as<string>();
        } else {
            out_file_name = input_file_name + "repacked.zip";
        }

        fs::path in_path(input_file_name);
        fs::path content_path(conent_file_name);
        fs::path out_path(out_file_name);

        if(!fs::exists(input_file_name)) {
            cerr << "Input zip not exists" << std::endl;
            return 0;
        }

        if(!fs::exists(content_path)) {
            cerr << "Content file not exists" << std::endl;
            return 0;
        }

        repack_zip(in_path, content_path, out_path);

    } catch (const cxxopts::OptionException& e) {
        cout << "error parsing options: " << e.what() << endl;
        return -1;
    } catch (std::ios_base::failure& e) {
        std::cout << "Caught an ios_base::failure.\n"
                  << "Explanatory string: " << e.what() << '\n'
                  << "Error code: " << e.code() << '\n';
        return -1;
    } catch (std::runtime_error& e) {
        std::cout << "Caught an runtime_error.\n"
                  << "Explanatory string: " << e.what() << '\n';
        return -1;
    }

    return 0;
}

int repack_zip(const fs::path& input_zip, const fs::path& content_file, const fs::path& out_zip)
{
    mz_bool status;
    mz_zip_archive zip_archive = {};

    status = mz_zip_reader_init_file(&zip_archive,
                                     (const char*)input_zip.string().c_str(),
                                     MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
    if (!status)
    {
        cerr << "mz_zip_reader_init_file() failed!" << endl;
        return -1;
    }

    char eif_name[256];
    status = mz_zip_reader_get_filename(&zip_archive,0, eif_name, sizeof(eif_name));
    if (!status)
    {
        cerr << "mz_zip_reader_get_filename() failed!" << endl;
        return -1;
    }

    // Close the archive, freeing any resources it was using
    mz_zip_reader_end(&zip_archive);

    ifstream file(content_file, ios::binary | ios::ate);
    if(file.fail()) {
        cerr << "Open content file error" << endl;
        return -1;
    }
    ifstream::pos_type pos = file.tellg();

    vector<uint8_t> file_buff(pos);
    file.seekg(0, ios::beg);
    file.read(reinterpret_cast<char *>(&file_buff[0]), file_buff.size());
    if (!file) {
        cerr << "Read file error, only " << file.gcount() << " could be read" << endl;
        file.close();
        return -1;
    }
    file.close();

    zip_archive = {};
    unsigned flags = MZ_DEFAULT_LEVEL | MZ_ZIP_FLAG_ASCII_FILENAME;
    status = mz_zip_writer_init_file_v2(&zip_archive, (const char*)out_zip.string().c_str(), 0, flags);
    if (!status) {
        cerr << "mz_zip_writer_init_file_v2 failed!";
        return -1;
    }

    status = mz_zip_writer_add_mem_ex(&zip_archive, eif_name, (void*)file_buff.data(), file_buff.size(), "", 0, flags, 0, 0);
    if (!status) {
        cerr << "mz_zip_writer_add_mem_ex failed!";
        return -1;
    }

    status = mz_zip_writer_finalize_archive(&zip_archive);
    if (!status) {
        cerr << "mz_zip_writer_finalize_archive failed!";
        return -1;
    }
    status = mz_zip_writer_end(&zip_archive);
    if (!status) {
        cerr << "mz_zip_writer_end failed!";
        return -1;
    }

    return EXIT_SUCCESS;
}