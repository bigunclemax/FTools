//
// Created by bigun on 12/28/2018.
//

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "EasyBMP/EasyBMP.h"

#ifndef VBFEDIT_EIFIMAGE_H
#define VBFEDIT_EIFIMAGE_H

namespace fs = std::filesystem;
namespace EIF {

static const uint8_t EIF_SIGNATURE[] = {'E','B','D',0x10,'E','I','F'};
static const int EIF_MULTICOLOR_PALETTE_SIZE = 0x300;
static const int EIF_MULTICOLOR_NUM_COLORS = 256;

enum EIF_TYPE : uint8_t {
    EIF_TYPE_MONOCHROME = 0x04,
    EIF_TYPE_MULTICOLOR = 0x07,
    EIF_TYPE_SUPERCOLOR = 0x0E
};

struct EifBaseHeader {
    uint8_t  signature[7];
    uint8_t  type;
    uint32_t length;
    uint16_t width;
    uint16_t height;
};

class EifImageBase {
protected:
    unsigned width;
    unsigned height;
    std::vector<uint8_t> bitmap_data;
    std::vector<uint8_t> palette;
    EIF_TYPE type;
public:
    virtual int openEif(const std::vector<uint8_t>& data) = 0;
    virtual int openBmp(std::string fileName) = 0;
    virtual void saveBmp(std::string fileName) = 0;
    virtual void saveEif(std::string fileName);
    virtual void saveEifToVector(std::vector<uint8_t>& data);
    virtual ~EifImageBase()= default;
};

class EifImage8bit: public EifImageBase {
public:
    EifImage8bit() { type = EIF_TYPE_MONOCHROME; };
    int openEif(const std::vector<uint8_t>& data) override;
    void saveBmp(std::string file_name) override;
    int openBmp(std::string file_name) override;
};

class EifImage16bit: public EifImageBase {
    uint8_t searchPixel(RGBApixel rgb_pixel);
public:
    EifImage16bit() { type = EIF_TYPE_MULTICOLOR; };
    EifImage16bit(unsigned int w, unsigned int h, const std::vector<uint8_t> &pal,
                                 const std::vector<uint8_t> &bitmap);
    int openEif(const std::vector<uint8_t>& data) override;
    void saveBmp(std::string file_name) override;
    int openBmp(std::string file_name) override;
    int setPalette(const std::vector<uint8_t>& data);
    void savePalette(const std::string& file_name);
    int setBitmap(const std::vector<uint8_t>& mapped_data);
    void getBitmap(std::vector<uint8_t>& data);
};

class EifImage32bit: public EifImageBase {
public:
    EifImage32bit() { type = EIF_TYPE_SUPERCOLOR; };
    int openEif(const std::vector<uint8_t>& data) override;
    int openBmp(std::string file_name) override;
    void saveBmp(std::string file_name) override;
};

class EifConverter {

public:
    static void eifToBmpFile(const std::vector<uint8_t>& data, const std::string& out_file_name,
            const std::string& palette_file_name = "");
    static void bmpFileToEifFile(const std::string& file_name, uint8_t depth, const std::string& out_file_name,
            const std::string& palette_file_name = "");
    static int createMultipaletteEifs(const fs::path& bmp_files, const fs::path& out_dir);
    static int createMultipaletteEifs(const std::vector<EifImage16bit*>& eifs);
};

}
#endif //VBFEDIT_EIFIMAGE_H

