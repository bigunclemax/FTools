#include <iostream>
#include <streambuf>

#include <cxxopts.hpp>
#include "VbfeditConfig.h"
#include "VbfFile.h"


using namespace std;

int main(int argc, char **argv)
{
    try {
        bool pack = false;
        bool unpack = false;

        cxxopts::Options options("VBFEditor", "Simple console VBF files unpacker/packer");
        options.add_options()
                ("p,pack","Pack VBF file", cxxopts::value<bool>(pack))
                ("u,unpack","Unpack VBF file", cxxopts::value<bool>(unpack))
                ("i,input","Input file", cxxopts::value<string>())
                ("o,output","Output directory", cxxopts::value<string>()->default_value(""))
                ("v,version","Print version")
                ("h,help","Print help");

        options.parse_positional({"input"});

        auto result = options.parse(argc, argv);

        if(result.arguments().empty() || result.count("help")){
            cout << options.help() << std::endl;
            return 0;
        }

        if(result.count("version")){
            cout << "VbfEdit version: "
            << Vbfedit_VERSION_MAJOR
            << Vbfedit_VERSION_MINOR
            << std::endl;
            return 0;
        }

        if(!result.count("input")){
            cout << "Please, specify input file" << std::endl;
            return 0;
        }

        if (pack){
            cout << "Sry, pack currently not supported" << endl;
            return 0;
        }

        if(unpack){
            VbfFile vbf;
            vbf.OpenFile(result["input"].as<string>());
            if(vbf.IsOpen())
                vbf.Export(result["output"].as<string>());
            else
                cout << "open error" << endl;

            return 0;
        }

        cout << options.help() << std::endl;

    } catch (const cxxopts::OptionException& e){
        cout << "error parsing options: " << e.what() << endl;
    }

    return 0;
}
