//
// Created by bigun on 12/23/2018.
//
#include <iostream>
#include <vector>
#include <cxxopts.hpp>
#include <utils.h>

#include "EifConverter.h"

using namespace std;

int main(int argc, char **argv)
{
    try {

        bool pack = false;
        bool unpack = false;
        unsigned depth = 32;

        cxxopts::Options options("eifconverter", "Ford EDB.EIF converter");
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

            if(out_file_name.empty()){
                out_file_name = input_file_name + ".bmp";
            }

            vector<uint8_t> v;
            FTUtils::fileToVector(input_file_name, v);
            EIF::EifConverter::eifToBmpFile(v, out_file_name);

        } else if(pack) {

            if(out_file_name.empty()){
                out_file_name = input_file_name + ".eif";
            }

            if(depth != 8 && depth != 16 && depth != 32) {
                cout << "Incorrect color depth value. It's may be only 8|16|32" << std::endl;
                return 0;
            }

            EIF::EifConverter::bmpFileToEifFile(input_file_name, depth, out_file_name);
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

