#include <iostream>
#include <streambuf>

#include <cxxopts.hpp>
#include <iomanip>
#include "VbfeditConfig.h"
#include "VbfFile.h"


using namespace std;

int main(int argc, char **argv)
{
    try {
        bool pack = false;
        bool unpack = false;
        bool info = false;

        cxxopts::Options options("VBFEditor", "Simple console VBF files unpacker/packer");
        options.add_options()
                ("p,pack","Pack VBF file", cxxopts::value<bool>(pack))
                ("u,unpack","Unpack VBF file", cxxopts::value<bool>(unpack))
                ("I,info","Show info about VBF file", cxxopts::value<bool>(info))
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
            VbfFile vbf;
            vbf.Import(result["input"].as<string>());
            if(vbf.IsOpen())
                vbf.SaveToFile(result["output"].as<string>());
            else
                cout << "import error" << endl;

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

        if(info){
            VbfFile vbf;
            vbf.OpenFile(result["input"].as<string>());
            if(vbf.IsOpen()) {
                auto sections_count = vbf.SectionsCount();
                cout << "Found " << sections_count << " sections" << endl;

                if(sections_count) {
                    cout
                    << "  #"
                    << " | "
                    << "   Offset   "
                    << " | "
                    << " Start addr "
                    << " | "
                    << " Length "
                    << endl;

                    VbfFile::SectionInfo section_info = {};
                    uint32_t hex_off = vbf.HeaderSz();

                    for(int i=0; i < sections_count; ++i) {
                        vbf.GetSectionInfo(i, section_info);
                        cout << resetiosflags(ios::hex)  << std::setfill(' ')
                            << setw(3) << i
                            << " | "
                            << hex << " 0x" << std::setfill('0') << setw(8) << hex_off << " "
                            << " | "
                            << hex << " 0x" << std::setfill('0') << setw(8) << section_info.start_addr << " "
                            << " | "
                            << " 0x" << setw(8) << section_info.length << resetiosflags(ios::hex)
                            <<" (" << section_info.length << ')'
                            << endl;

                        hex_off += section_info.length + sizeof(uint32_t) * 2 + sizeof(uint16_t);
                    }
                }
            }
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
