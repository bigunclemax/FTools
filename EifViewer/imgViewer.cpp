//
// Created by bigun on 12/23/2018.
//
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cxxopts.hpp>
#include <exception>

#include "EifImage.h"

using namespace std;

void saveImage(EifImageBase* img, string file_name) {
    img->saveBmp(file_name);
}

int main(int argc, char **argv)
{
    try {

        bool save = false;
        bool view = false;

        cxxopts::Options options("eif2bitmap", "Ford EDB.EIF viewer ");
        options.add_options()
                ("s,save","Save bitmap image to file", cxxopts::value<bool>(save))
                ("v,view","Print ASCII representation", cxxopts::value<bool>(view))
                ("i,input","Input file", cxxopts::value<string>())
                ("o,output","Output directory", cxxopts::value<string>()->default_value(""))
                ("h,help","Print help");

        auto result = options.parse(argc, argv);

        if(result.arguments().empty() || result.count("help")){
            cout << options.help() << std::endl;
            return 0;
        }

        if(!result.count("input")){
            cout << "Please, specify input file" << std::endl;
            return 0;
        }

        auto file_name = result["input"].as<string>();
        ifstream in_file(file_name, ios::binary | ios::ate);
        if(in_file.fail())
            throw runtime_error("File open error");
//        in_file.exceptions(in_file.failbit); gcc sucks

        auto pos = in_file.tellg();
        in_file.seekg(0, ios::beg);

        vector<uint8_t> file_content(pos);
        in_file.read(reinterpret_cast<char *>(&file_content[0]), file_content.size());
//        in_file.exceptions(in_file.failbit); gcc sucks
        if(in_file.fail())
            throw runtime_error("File open error");
        in_file.close();

        EifImageBase* img;
        EifImageMonochrome mono_img;
        EifImageMulticolor colur_img;
        EifImageMegacolor mega_img;
        if(file_content[7] == EIF_TYPE_MONOCHROME) {
            mono_img.openImage(file_content);
            img = &mono_img;
        } else if(file_content[7] == EIF_TYPE_MULTICOLOR) {
                colur_img.openImage(file_content);
                img = &colur_img;
        } else if(file_content[7] == EIF_TYPE_SUPERCOLOR){
            mega_img.openImage(file_content);
            img = &mega_img;
        } else {
                throw runtime_error("unsupported format");
        }

        if(save) {
            img->saveBmp(file_name);
        }

        if(view) {
            img->printAscii();
        }

    } catch (const cxxopts::OptionException& e){
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

