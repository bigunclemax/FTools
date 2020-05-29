//
// Created by bigun on 12/28/2018.
//

#include "EifConverter.h"
#include <fstream>
#include <limits>
#include <utils.h>

using namespace EIF;

int EifImage8bit::openBmp(std::string file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.c_str());

    auto depth = bmp_image.TellBitDepth();
    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();

    auto aligned_width = (width % 4) ? (width/4 + 1)*4 : width;
    bitmap_data.clear();
    bitmap_data.resize(height * aligned_width);

    for(auto i =0; i < height; i++){
        for(auto j=0; j < aligned_width; j++){
            uint8_t pixel;
            if(j >= width) {
                pixel = 0x0;
            } else {
                RGBApixel rgb_pixel = bmp_image.GetPixel(j,i);
                pixel = (uint8_t)((rgb_pixel.Red + rgb_pixel.Green + rgb_pixel.Blue)/3);
            }
            bitmap_data[i * aligned_width + j] = pixel;
        }
    }

    return 0;
}

void EifImage8bit::saveEif(std::string file_name) {
    /* create header */
    std::vector<uint8_t> eif_img(sizeof(EifBaseHeader));
    auto& header = *reinterpret_cast<struct EifBaseHeader*>(&eif_img[0]);

    /* copy signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        eif_img[i] = EIF_SIGNATURE[i];
    }

    /* set type */
    header.type = EIF_TYPE_MONOCHROME;

    /* set height */
    header.height = (uint16_t)height;

    /* set width */
    header.width = (uint16_t)width;
    auto aligned_width = (width % 4) ? (width/4 + 1)*4 : width;

    /* set length */
    header.length = aligned_width * header.height;

    /* set pixels */
    eif_img.insert(std::end(eif_img), std::begin(bitmap_data), std::end(bitmap_data));

    std::ofstream out_file(file_name, std::ios::out | std::ios::binary);
    if (!out_file) {
        std::cout << "file: " << file_name << " can't be created" << std::endl;
    } else {
        out_file.write(reinterpret_cast<char *>(&eif_img[0]), eif_img.size());
    }
    out_file.close();
}

void EifImage8bit::saveBmp(std::string file_name){

    BMP bmp_image;
    bmp_image.SetSize((int)width, (int)height);

    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            auto color = *(&bitmap_data[0] + i * width + j);

            RGBApixel rgb_pixel;
            rgb_pixel.Alpha = 0xFF;
            rgb_pixel.Red = color;
            rgb_pixel.Green = color;
            rgb_pixel.Blue = color;

            bmp_image.SetPixel(j, i, rgb_pixel);
        }
    }

    bmp_image.WriteToFile(file_name.c_str());
}

int EifImage8bit::openEif(const std::vector<uint8_t>& data) {

    if(data.size() < sizeof(EifBaseHeader)){
        throw std::runtime_error("EIF wrong header length");
    }
    const EifBaseHeader &header = *reinterpret_cast<const struct EifBaseHeader*>(&data[0]);

    /* check signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        if(EIF_SIGNATURE[i] != header.signature[i]){
            throw std::runtime_error("EIF wrong signature");
        }
    }

    /* check type */
    if(EIF_TYPE_MONOCHROME != header.type){
        throw std::runtime_error("EIF wrong type");
    }

    auto data_offset = sizeof(EifBaseHeader);
    if(header.length > (data.size() - data_offset)){
        throw std::runtime_error("EIF wrong data length");
    }

    //get aligned width
    auto aligned4_width = header.length / header.height;
    if(aligned4_width % sizeof(uint32_t)){
        throw std::runtime_error("EIF not aligned width");
    }

    if(aligned4_width < header.width){
        throw std::runtime_error("EIF wrong actual width");
    }

    bitmap_data.clear();
    bitmap_data.reserve(header.height * header.width);
    for (auto i =data_offset; i < data.size(); i+=aligned4_width) {

        bitmap_data.insert(std::end(bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width);
    }

    width = header.width;
    height = header.height;

    return 0;
}

int EifImage16bit::openEif(const std::vector<uint8_t> &data) {

    if(data.size() < sizeof(EifBaseHeader)){
        throw std::runtime_error("EIF wrong header length");
    }
    const EifBaseHeader &header = *reinterpret_cast<const struct EifBaseHeader*>(&data[0]);

    /* check signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        if(EIF_SIGNATURE[i] != header.signature[i]){
            throw std::runtime_error("EIF wrong signature");
        }
    }

    /* check type */
    if(EIF_TYPE_MULTICOLOR != header.type){
        throw std::runtime_error("EIF wrong type");
    }

    auto data_offset = sizeof(EifBaseHeader) + EIF_MULTICOLOR_PALETTE_SIZE;
    if(header.length > (data.size() - data_offset)){
        throw std::runtime_error("EIF wrong data length");
    }

    //get aligned width
    auto aligned16_width = header.length / header.height;
    if(aligned16_width % sizeof(uint16_t)){
        throw std::runtime_error("EIF not aligned width");
    }

    if((aligned16_width / sizeof(uint16_t)) < header.width){
        throw std::runtime_error("EIF wrong actual width");
    }

    bitmap_data.clear();
    bitmap_data.reserve(header.height * header.width);
    for (auto i =data_offset; i < data.size(); i+=aligned16_width) {
        bitmap_data.insert(std::end(bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width * sizeof(uint16_t));
    }

    palette.resize(EIF_MULTICOLOR_PALETTE_SIZE);
    std::copy(
            data.begin() + sizeof(EifBaseHeader),
            data.begin() + sizeof(EifBaseHeader) + EIF_MULTICOLOR_PALETTE_SIZE,
            palette.begin());

    width = header.width;
    height = header.height;

    return 0;
}

uint8_t EifImage16bit::searchPixel(RGBApixel rgb_pixel) {

    double color_distance = std::numeric_limits<double>::max();
    uint8_t closest_index =0;

    for(auto i=0; i < EIF_MULTICOLOR_PALETTE_SIZE; i+=3) {
        int R = palette[i];
        int G = palette[i+1];
        int B = palette[i+2];

        double color_distance_new = sqrt(
                pow((rgb_pixel.Blue - B),2) +
                pow((rgb_pixel.Red - R),2) +
                pow((rgb_pixel.Green - G),2));

        if(color_distance_new < color_distance) {
            color_distance = color_distance_new;
            closest_index = (uint8_t)(i / 3);
            if(color_distance == 0) {
                break;
            }
        }
    }

    return closest_index;
}

int EifImage16bit::openBmp(std::string file_name) {

    if(palette.empty()) {
        std::cerr << "Can't open bmp file: " << file_name << " Palette required" << std::endl;
        return 0;
    }

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.c_str());

    auto depth = bmp_image.TellBitDepth();
    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();

    auto aligned_width = (width % 2) ? (width/2 + 1)*2 : width;
    bitmap_data.clear();
    bitmap_data.resize(height * aligned_width * 2);

    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){ //TODO: maybe this need aligned_width ?
            if(j >= width){
                bitmap_data[0 + i * width *2 + j*2] = 0x0;
                bitmap_data[1 + i * width *2 + j*2] = 0x0;
            } else {
                RGBApixel rgb_pixel = bmp_image.GetPixel(j,i);
                bitmap_data[0 + i * width *2 + j*2] = searchPixel(rgb_pixel);
                bitmap_data[1 + i * width *2 + j*2] = rgb_pixel.Alpha;
            }
        }
    }

    return 0;
}

void EifImage16bit::saveEif(std::string file_name) {
    /* create header */
    std::vector<uint8_t> eif_img(sizeof(EifBaseHeader));
    auto& header = *reinterpret_cast<struct EifBaseHeader*>(&eif_img[0]);

    /* copy signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        eif_img[i] = EIF_SIGNATURE[i];
    }

    /* set type */
    header.type = EIF_TYPE_MULTICOLOR;

    /* set height */
    header.height = (uint16_t)height;

    /* set width */
    header.width = (uint16_t)width;

    /* set length */
    header.length = bitmap_data.size();

    /* set palette */
    for(int i=0; i < EIF_MULTICOLOR_PALETTE_SIZE; i++) {
        eif_img.push_back(palette[i]);
    }

    /* set pixels */
    eif_img.insert(std::end(eif_img), std::begin(bitmap_data), std::end(bitmap_data));

    std::ofstream out_file(file_name, std::ios::out | std::ios::binary);
    if (!out_file) {
        std::cout << "file: " << file_name << " can't be created" << std::endl;
    } else {
        out_file.write(reinterpret_cast<char *>(&eif_img[0]), eif_img.size());
    }
    out_file.close();
}

void EifImage16bit::saveBmp(std::string file_name) {

    BMP bmp_image;
    bmp_image.SetBitDepth(32);
    bmp_image.SetSize((int)width, (int)height);

    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            RGBApixel rgb_pixel;
            auto color_idx = *(&bitmap_data[0] + i * width *2 + j*2);
            rgb_pixel.Alpha = *(&bitmap_data[0] + 1 + i * width *2 + j*2);

            rgb_pixel.Red = palette[color_idx*3];
            rgb_pixel.Green = palette[color_idx*3+1];
            rgb_pixel.Blue = palette[color_idx*3+2];

            bmp_image.SetPixel(j, i, rgb_pixel);
        }
    }

    bmp_image.WriteToFile(file_name.c_str());
}

int EifImage16bit::setPalette(const std::vector<uint8_t> &data) {

    if(data.size() != EIF_MULTICOLOR_PALETTE_SIZE) {
        std::cerr << "Error: palette wrong size" << std::endl;
        return -1;
    }

    palette = data;

    return 0;
}

void EifImage16bit::savePalette(const std::string& file_name) {

    if(palette.size() != EIF_MULTICOLOR_PALETTE_SIZE) {
        std::cerr << "Error: palette wrong size" << std::endl;
        return;
    }

    FTUtils::bufferToFile(file_name, (char *)palette.data(), EIF_MULTICOLOR_PALETTE_SIZE);
}

int EifImage32bit::openEif(const std::vector<uint8_t> &data) {

    if(data.size() < sizeof(EifBaseHeader)){
        throw std::runtime_error("EIF wrong header length");
    }
    const EifBaseHeader &header = *reinterpret_cast<const struct EifBaseHeader*>(&data[0]);

    /* check signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        if(EIF_SIGNATURE[i] != header.signature[i]){
            throw std::runtime_error("EIF wrong signature");
        }
    }

    /* check type */
    if(EIF_TYPE_SUPERCOLOR != header.type){
        throw std::runtime_error("EIF wrong type");
    }

    auto data_offset = sizeof(EifBaseHeader);
    if(header.length > (data.size() - data_offset)){
        throw std::runtime_error("EIF wrong data length");
    }

    //get aligned width
    auto aligned4_width = header.length / header.height;
    if(aligned4_width % sizeof(uint32_t)){
        throw std::runtime_error("EIF not aligned width");
    }

    if((aligned4_width / sizeof(uint32_t)) < header.width){
        throw std::runtime_error("EIF wrong actual width");
    }

    bitmap_data.clear();
    bitmap_data.reserve(header.height * header.width);
    for (auto i =data_offset; i < data.size(); i+=aligned4_width) {

        bitmap_data.insert(std::end(bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width * sizeof(uint32_t));
    }

    width = header.width;
    height = header.height;

    return 0;
}

void EifImage32bit::saveBmp(std::string file_name) {

    BMP bmp_image;
    bmp_image.SetBitDepth(32);
    bmp_image.SetSize((int)width, (int)height);

    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){

            RGBApixel rgb_pixel;

            rgb_pixel.Alpha = *(&bitmap_data[0] + 3 + i * width *4 + j*4);
            rgb_pixel.Red   = *(&bitmap_data[0] + 2 + i * width *4 + j*4);
            rgb_pixel.Green = *(&bitmap_data[0] + 1 + i * width *4 + j*4);
            rgb_pixel.Blue  = *(&bitmap_data[0] + 0 + i * width *4 + j*4);

            bmp_image.SetPixel(j, i, rgb_pixel);
        }
    }
    bmp_image.WriteToFile(file_name.c_str());
}

int EifImage32bit::openBmp(std::string file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.c_str());

    auto depth = bmp_image.TellBitDepth();
    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();

    bitmap_data.clear();
    bitmap_data.resize(height * width * 4);

    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){

            RGBApixel rgb_pixel = bmp_image.GetPixel(j,i);

            bitmap_data[0 + 3 + i * width *4 + j*4] = rgb_pixel.Alpha;
            bitmap_data[0 + 2 + i * width *4 + j*4] = rgb_pixel.Red;
            bitmap_data[0 + 1 + i * width *4 + j*4] = rgb_pixel.Green;
            bitmap_data[0 + 0 + i * width *4 + j*4] = rgb_pixel.Blue;
        }
    }

    return 0;
}

void EifImage32bit::saveEif(std::string file_name) {

    /* create header */
    std::vector<uint8_t> eif_img(sizeof(EifBaseHeader));
    auto& header = *reinterpret_cast<struct EifBaseHeader*>(&eif_img[0]);

    /* copy signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        eif_img[i] = EIF_SIGNATURE[i];
    }

    /* set type */
    header.type = EIF_TYPE_SUPERCOLOR;

    /* set height */
    header.height = (uint16_t)height;

    /* set width */
    header.width = (uint16_t)width;

    /* set length */
    header.length = header.width * header.height * 4;

    /* set pixels */
    eif_img.insert(std::end(eif_img), std::begin(bitmap_data), std::end(bitmap_data));

    std::ofstream out_file(file_name, std::ios::out | std::ios::binary);
    if (!out_file) {
        std::cout << "file: " << file_name << " can't be created" << std::endl;
    } else {
        out_file.write(reinterpret_cast<char *>(&eif_img[0]), eif_img.size());
    }
    out_file.close();
}

void EifConverter::eifToBmpFile(const std::vector<uint8_t>& data, const std::string& out_file_name) {

    EifImageBase* image;

    if(data[7] == EIF_TYPE_MONOCHROME) {
        image = new EifImage8bit;
    } else if(data[7] == EIF_TYPE_MULTICOLOR) {
        image = new EifImage16bit;
    } else if(data[7] == EIF_TYPE_SUPERCOLOR){
        image = new EifImage32bit;
    } else {
        throw std::runtime_error("Unsupported EIF format");
    }

    image->openEif(data);
    image->saveBmp(out_file_name);
    if(data[7] == EIF_TYPE_MULTICOLOR) {
        dynamic_cast<EifImage16bit*>(image)->savePalette(out_file_name+".pal");
    }

    delete image;
}

void EifConverter::bmpFileToEifFile(const std::string& file_name, uint8_t depth, const std::string& out_file_name,
        const std::string& palette_file_name)
{

    EifImageBase* image;
    switch (depth) {
        case 8:
            image = new EifImage8bit;
            break;
        case 16: {
            image = new EifImage16bit;
            if(palette_file_name.empty()) {
                throw std::runtime_error("Incorrect palette path");
            }
            std::vector<uint8_t> palette;
            FTUtils::fileToVector(palette_file_name, palette);
            dynamic_cast<EifImage16bit*>(image)->setPalette(palette);
        }
            break;
        case 32:
            image = new EifImage32bit;
            break;
        default:
            throw std::runtime_error("Incorrect depth value");
    }

    image->openBmp(file_name);
    image->saveEif(out_file_name);

    delete image;
}
