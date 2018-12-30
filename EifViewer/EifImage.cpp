//
// Created by bigun on 12/28/2018.
//

#include "EifImage.h"

void EifImageMonochrome::printAscii() {

    auto i = 0;
    for(auto elem :bitmap_data){
        if((i++ % width) == 0){
            std::cout << std::endl;
        }
        if(elem){
            std::cout << std::hex
                      << std::setw(2)
                      << std::setfill(' ')
                      << (int)(elem);
        } else {
            std::cout << "  ";
        }
    }
}

void EifImageMonochrome::saveBmp(std::string fileName){

    //TODO: add color process
    bitmap_image bmp_image(width, height);
    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            auto pixel = *(&bitmap_data[0] + i * width + j);
            bmp_image.set_pixel(j, i, pixel,pixel,pixel);
        }
    }

    bmp_image.save_image(fileName + ".bmp");
}

int EifImageMonochrome::openImage(const std::vector<uint8_t>& data) {

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


int EifImageMulticolor::openImage(const std::vector<uint8_t> &data) {

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
                           data.begin() + i + header.width*sizeof(uint16_t));
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

void EifImageMulticolor::printAscii() {

}

void EifImageMulticolor::saveBmp(std::string fileName) {

    bitmap_image bmp_image(width, height);
    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            auto pixel = *(&bitmap_data[0] + i * width *2 + j*2);
            uint8_t R = palette[pixel*3];
            uint8_t G = palette[pixel*3+1];
            uint8_t B = palette[pixel*3+2];
            bmp_image.set_pixel(j, i, R,G,B);
        }
    }

    bmp_image.save_image(fileName + ".bmp");
}

int EifImageMegacolor::openImage(const std::vector<uint8_t> &data) {

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

void EifImageMegacolor::printAscii() {

}

void EifImageMegacolor::saveBmp(std::string fileName) {

    bitmap_image bmp_image(width, height);
    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){
            uint8_t B = *(&bitmap_data[0] + 0 + i * width *4 + j*4);
            uint8_t G = *(&bitmap_data[0] + 1 + i * width *4 + j*4);
            uint8_t R = *(&bitmap_data[0] + 2 + i * width *4 + j*4);
            uint8_t A = *(&bitmap_data[0] + 3 + i * width *4 + j*4);
            bmp_image.set_pixel(j, i, R,G,B);
        }
    }

    bmp_image.save_image(fileName + ".bmp");
}