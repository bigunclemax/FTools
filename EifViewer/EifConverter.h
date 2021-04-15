//
// Created by bigun on 12/28/2018.
//

#include <cstdint>
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "EasyBMP/EasyBMP.h"

#ifndef VBFEDIT_EIFIMAGE_H
#define VBFEDIT_EIFIMAGE_H

namespace fs = std::filesystem;
using namespace std;

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

struct BitmapData {
    vector<uint8_t> bitmapRGBA;
    vector<uint8_t> bitmapAlpha;
    unsigned width;
    unsigned height;
};

class EifImageBase {
protected:
    unsigned width =0;
    unsigned height =0;
    vector<uint8_t> m_bitmap_data;
    virtual void store_palette(vector<uint8_t>& data) const {};
    virtual void store_bitmap(vector<uint8_t> &data) const {data.insert(end(data), begin(m_bitmap_data), end(m_bitmap_data));};
public:
    virtual int openEif(const vector<uint8_t>& data) = 0;
    [[nodiscard]] virtual int getType() const = 0;
    virtual int openBmp(const fs::path& fileName) = 0;
    virtual void saveBmp(const fs::path& fileName) const = 0;
    virtual void saveEif(const fs::path& fileName) const;
    [[nodiscard]] virtual vector<uint8_t> saveEifToVector() const;
    virtual ~EifImageBase()= default;
    [[nodiscard]] virtual unsigned getWidth() const { return width; };
    [[nodiscard]] virtual unsigned getHeight() const { return height; };
    [[nodiscard]] virtual vector<uint8_t> getBitmapRBGA() const = 0;
};

class EifImage8bit: public EifImageBase {
    EIF_TYPE type = EIF_TYPE_MONOCHROME;
public:
    [[nodiscard]] inline int getType() const override { return type; };
    int openEif(const vector<uint8_t>& data) override;
    void saveBmp(const fs::path& file_name) const override;
    int openBmp(const fs::path& file_name) override;
    [[nodiscard]] vector<uint8_t> getBitmapRBGA() const override;
};

class EifImage16bit: public EifImageBase {
    EIF_TYPE type = EIF_TYPE_MULTICOLOR;
    vector<uint8_t> m_palette;
    void store_palette(vector<uint8_t> &data) const override;
    void store_bitmap(vector<uint8_t> &data) const override;
public:
    [[nodiscard]] inline int getType() const override { return type; };
    EifImage16bit() = default;
    int openEif(const vector<uint8_t>& data) override;
    void saveBmp(const fs::path& file_name) const override;
    int openBmp(const fs::path& file_name) override;
    int setPalette(const vector<uint8_t>& data);
    void savePalette(const fs::path& file_name);
    [[nodiscard]] vector<uint8_t> getBitmapRBGA() const override;
};

class EifImage32bit: public EifImageBase {
    EIF_TYPE type = EIF_TYPE_SUPERCOLOR;
public:
    [[nodiscard]] inline int getType() const override { return type; };
    int openEif(const vector<uint8_t>& data) override;
    int openBmp(const fs::path& file_name) override;
    void saveBmp(const fs::path& file_name) const override;
    [[nodiscard]] vector<uint8_t> getBitmapRBGA() const override;
};

class EifConverter {
public:
    static void mapMultiPalette(vector<EifImage16bit>& eifs);
    static void eifToBmpFile(const vector<uint8_t>& data, const fs::path& out_file_name,
            const fs::path& palette_file_name = "", bool store_palette = false);
    static void bmpFileToEifFile(const fs::path& file_name, uint8_t depth, const fs::path& out_file_name,
            const fs::path& palette_file_name = "");
    static int bulkPack(const fs::path& bmp_files, const fs::path& out_dir);
    static std::unique_ptr<EifImageBase> makeEif(const vector<uint8_t>& data);
};

}
#endif //VBFEDIT_EIFIMAGE_H

