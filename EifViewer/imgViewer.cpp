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

int main(int argc, char **argv)
{
    try {

        bool pack = false;
        bool unpack = false;
        unsigned depth = 32;

        cxxopts::Options options("eif2bitmap", "Ford EDB.EIF viewer ");
        options.add_options()
                ("p,pack","Pack VBF file", cxxopts::value<bool>(pack))
                ("u,unpack","Unpack VBF file", cxxopts::value<bool>(unpack))
                ("d,depth","Output Eif type 8/16/32", cxxopts::value<unsigned>(depth))
                ("i,input","Input file", cxxopts::value<string>())
                ("o,output","Output file", cxxopts::value<string>())
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

        string out_file_name;
        if(result.count("output")){
            out_file_name = result["output"].as<string>();
        }

        if(unpack) {
            ifstream in_file(input_file_name, ios::binary | ios::ate);
            if(in_file.fail())
                throw runtime_error("File open error");

            auto pos = in_file.tellg();
            in_file.seekg(0, ios::beg);

            vector<uint8_t> file_content(pos);
            in_file.read(reinterpret_cast<char *>(&file_content[0]), file_content.size());
            if(in_file.fail())
                throw runtime_error("File open error");
            in_file.close();

            if(out_file_name.empty()){
                out_file_name = input_file_name + ".bmp";
            }

            EifImageBase* image;

            if(file_content[7] == EIF_TYPE_MONOCHROME) {
                image = new EifImage8bit;
            } else if(file_content[7] == EIF_TYPE_MULTICOLOR) {
                image = new EifImage16bit;
            } else if(file_content[7] == EIF_TYPE_SUPERCOLOR){
                image = new EifImage32bit;
            } else {
                throw runtime_error("unsupported format");
            }

            image->openEif(file_content);
            image->saveBmp(out_file_name);

            delete image;

        } else if(pack) {

            if(out_file_name.empty()){
                out_file_name = input_file_name + ".eif";
            }

            EifImageBase* image;
            switch (depth) {
                case 8:
                    image = new EifImage8bit;
                    break;
                case 16:
                    image = new EifImage16bit;
                    break;
                case 32:
                    image = new EifImage32bit;
                    break;
                default:
                    throw runtime_error("Incorrect depth value");
            }

            image->openBmp(input_file_name);
            image->saveEif(out_file_name);

            delete image;
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

