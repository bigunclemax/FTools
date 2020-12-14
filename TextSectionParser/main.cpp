//
// Created by user on 06.12.2020.
//
#include <cxxopts.hpp>
#include <string>
#include <filesystem>
#include "TextSectionPacker.h"

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char **argv)
{
    try {

        bool unpack = false;
        bool pack = false;

        cxxopts::Options options("textparser", "Ford IPC text resources extractor");
        options.add_options()
                ("u,unpack","Extract text resources form to destination dir", cxxopts::value<bool>(unpack))
                ("p,pack","Pack text section", cxxopts::value<bool>(pack))
                ("i,input","Input file", cxxopts::value<string>())
                ("o,output","Output directory", cxxopts::value<string>()->default_value(""))
                ("h,help","Print help");

        options.parse_positional({"input"});
        auto result = options.parse(argc, argv);

        if(result.arguments().empty() || result.count("help")){
            cerr << options.help() << endl;
            return -1;
        }

        if(!result.count("input")){
            cerr << "please, specify input file" << endl;
            return -1;
        }
        fs::path in_path = result["input"].as<string>();
        fs::path out_path = fs::current_path();
        if(result.count("output")) {
            out_path = result["output"].as<string>();
        }

        if(pack) {
            TextSectionPacker::pack(in_path, out_path);
        } else if(unpack) {
            TextSectionPacker::unpack(FTUtils::fileToVector(in_path), out_path);
        } else {
            cerr << options.help() << endl;
            return -1;
        }

    } catch (const cxxopts::OptionException& e){
        cerr << "error parsing options: " << e.what() << endl;
    }

    return 0;
}

