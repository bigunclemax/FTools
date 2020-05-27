//
// Created by user on 25.05.2020.
//
#include <cxxopts.hpp>
#include "ImageSection.h"

int main(int argc, char **argv)
{
    try {

        bool unpack = false;
        bool pack = false;
        bool verbose = false;

        cxxopts::Options options("imgparcer", "Ford IPC images extractor");
        options.add_options()
                ("u,unpack","Extract resources form image section to destination dir", cxxopts::value<bool>(unpack))
                ("p,pack","Pack image section", cxxopts::value<bool>(pack))
                ("i,input","Input file", cxxopts::value<string>())
//                ("v,verbose","Show section content", cxxopts::value<bool>(verbose)) //TODO: add verbose mode
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
        string in_path = result["input"].as<string>();
        string out_path = result["output"].as<string>();
        if(out_path.empty()) {
            out_path = (fs::current_path() / "patched.bin").string();
        } else {
            fs::path p_out(out_path);
            if(!fs::is_directory(p_out) && !fs::is_directory(p_out.parent_path())) {
                cerr << "output dir not exists" << endl;
                return -1;
            }
        }

        if(pack) {
            ImageSection section;
            section.Import(in_path);
            section.SaveToFile(out_path);
        } else if(unpack) {
            ImageSection section;
            section.ParseFile(in_path);
            section.Export(out_path);
        } else {
            cerr << options.help() << endl;
            return -1;
        }

    } catch (const cxxopts::OptionException& e){
        cerr << "error parsing options: " << e.what() << endl;
    }

    return 0;
}

