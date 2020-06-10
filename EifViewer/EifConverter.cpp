//
// Created by bigun on 12/28/2018.
//

#include "EifConverter.h"
#include <fstream>
#include <limits>
#include <utils.h>
#include <exoquant.h>

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

int EifImage16bit::openBmp(std::string file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.c_str());

    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();
    auto num_pixels = height * width;

    bitmap_data.clear();
    bitmap_data.resize(num_pixels * 2);

    std::vector<uint8_t> pImage_data;
    pImage_data.reserve(num_pixels*4);
    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            auto px = bmp_image.GetPixel(j,i);
            pImage_data.push_back(px.Red);
            pImage_data.push_back(px.Green);
            pImage_data.push_back(px.Blue);
            pImage_data.push_back(px.Alpha);
        }
    }

    unsigned char pPalette[EIF_MULTICOLOR_NUM_COLORS * 4];
    unsigned char* pOut = (unsigned char*)malloc(num_pixels);

    exq_data *pExq;
    pExq = exq_init();
    exq_no_transparency(pExq);
    exq_feed(pExq, (unsigned char *)pImage_data.data(), num_pixels);
    exq_quantize_hq(pExq, EIF_MULTICOLOR_NUM_COLORS); //TODO: add param for fast mode
    exq_get_palette(pExq, pPalette, EIF_MULTICOLOR_NUM_COLORS);
    exq_map_image(pExq, num_pixels, (unsigned char *)pImage_data.data(), pOut);
    exq_free(pExq);

    //convert palette
    palette.resize(EIF_MULTICOLOR_NUM_COLORS * 3);
    for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; i++) {
        palette[i*3+0] = pPalette[i*4+0];
        palette[i*3+1] = pPalette[i*4+1];
        palette[i*3+2] = pPalette[i*4+2];
    }

    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            bitmap_data[0 + i * width *2 + j*2] = pOut[i*width+j];
            bitmap_data[1 + i * width *2 + j*2] = 0xFF; //TODO: add transparency support
        }
    }

    free(pOut);

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

    delete image;
}

void EifConverter::bmpFileToEifFile(const std::string& file_name, uint8_t depth, const std::string& out_file_name) {

    EifImageBase* image;
    switch (depth) {
        case 8:
            image = new EifImage8bit;
            break;
        case 16:
            image = new EifImage16bit;
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
