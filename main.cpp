#include <winsock2.h>

#include <iostream>
#include <fstream>
#include <streambuf>
#include <list>
#include <regex>

#include "VbfFile.h"


using namespace std;



int main(int argc, char **argv)
{
    if(argc < 2){
        cout << "Pls, specify file" << endl;
        return 0;
    }

    VbfFile vbf;
    vbf.OpenFile(argv[1]);
    if(vbf.IsOpen())
        vbf.Export("./");
    else
        cout << "open error" << endl;

    return 0;
}
