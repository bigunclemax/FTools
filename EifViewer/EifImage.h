//
// Created by bigun on 12/28/2018.
//

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <bitmap_image.hpp>

#ifndef VBFEDIT_EIFIMAGE_H
#define VBFEDIT_EIFIMAGE_H

static const uint8_t EIF_SIGNATURE[] = {'E','B','D',0x10,'E','I','F'};
static const uint8_t EIF_TYPE_MONOCHROME = 0x04;
static const uint8_t EIF_TYPE_MULTICOLOR = 0x07;
static const uint8_t EIF_TYPE_SUPERCOLOR = 0x0E;
static const int EIF_MULTICOLOR_PALETTE_SIZE = 0x300;

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
public:
    virtual int openImage(const std::vector<uint8_t>& data) = 0;
    virtual void printAscii() = 0;
    virtual void saveBmp(std::string fileName) = 0;
    virtual ~EifImageBase(){};
    static uint8_t alfaComposing(uint8_t c1, uint8_t a1, uint8_t c2 = 0xFF, uint8_t a2 = 0xFF);
};

class EifImageMonochrome: public EifImageBase {
//    uint8_t base_color;
public:
    int openImage(const std::vector<uint8_t>& data) override;
    void printAscii() override;
    void saveBmp(std::string fileName) override;
};

class EifImageMulticolor: public EifImageBase {
    std::vector<uint8_t> palette; //TODO:
public:
    int openImage(const std::vector<uint8_t>& data) override;
    void printAscii() override;
    void saveBmp(std::string fileName) override;
};

class EifImageMegacolor: public EifImageBase {
public:
    int openImage(const std::vector<uint8_t>& data) override;
    void printAscii() override;
    void saveBmp(std::string fileName) override;
};

#endif //VBFEDIT_EIFIMAGE_H

