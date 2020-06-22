//
// Created by bigun on 12/28/2018.
//

#include "EifConverter.h"
#include <fstream>
#include <limits>
#include <utils.h>
#include <exoquant.h>
#include <algorithm>

using namespace EIF;

void EifImageBase::saveEifToVector(vector<uint8_t>& data) {

    /* create header */
    data.resize(sizeof(EifBaseHeader));
    auto& header = *reinterpret_cast<struct EifBaseHeader*>(data.data());

    /* copy signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        data[i] = EIF_SIGNATURE[i];
    }

    /* set type */
    header.type = getType();

    /* set height */
    header.height = (uint16_t)height;

    /* set width */
    header.width = (uint16_t)width;

    /* set length */
    header.length = bitmap_data.size();

    /* set palette */
    store_palette(data);

    /* set pixels */
    data.insert(end(data), begin(bitmap_data), end(bitmap_data));
}

void EifImageBase::saveEif(const fs::path& file_name) {
    vector<uint8_t> eif_img;
    saveEifToVector(eif_img);
    FTUtils::bufferToFile(file_name, (char *)eif_img.data(), eif_img.size());
}

int EifImage8bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.string().c_str());

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

void EifImage8bit::saveBmp(const fs::path& file_name){

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

    bmp_image.WriteToFile(file_name.string().c_str());
}

int EifImage8bit::openEif(const vector<uint8_t>& data) {

    if(data.size() < sizeof(EifBaseHeader)){
        throw runtime_error("EIF wrong header length");
    }
    const EifBaseHeader &header = *reinterpret_cast<const struct EifBaseHeader*>(&data[0]);

    /* check signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        if(EIF_SIGNATURE[i] != header.signature[i]){
            throw runtime_error("EIF wrong signature");
        }
    }

    /* check type */
    if(EIF_TYPE_MONOCHROME != header.type){
        throw runtime_error("EIF wrong type");
    }

    auto data_offset = sizeof(EifBaseHeader);
    if(header.length > (data.size() - data_offset)){
        throw runtime_error("EIF wrong data length");
    }

    //get aligned width
    auto aligned4_width = header.length / header.height;
    if(aligned4_width % sizeof(uint32_t)){
        throw runtime_error("EIF not aligned width");
    }

    if(aligned4_width < header.width){
        throw runtime_error("EIF wrong actual width");
    }

    bitmap_data.clear();
    bitmap_data.reserve(header.height * header.width);
    for (auto i =data_offset; i < data.size(); i+=aligned4_width) {

        bitmap_data.insert(end(bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width);
    }

    width = header.width;
    height = header.height;

    return 0;
}

EifImage16bit::EifImage16bit(unsigned int w, unsigned int h, const vector<uint8_t> &pal,
                             const vector<uint8_t> &bitmap)
{
    width = w;
    height = h;
    palette = pal;
    bitmap_data = bitmap;
}

void EifImage16bit::store_palette(vector<uint8_t> &data) {
    for(int i=0; i < EIF_MULTICOLOR_PALETTE_SIZE; i++) {
        data.push_back(palette[i]);
    }
}

int EifImage16bit::openEif(const vector<uint8_t> &data) {

    if(data.size() < sizeof(EifBaseHeader)){
        throw runtime_error("EIF wrong header length");
    }
    const EifBaseHeader &header = *reinterpret_cast<const struct EifBaseHeader*>(&data[0]);

    /* check signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        if(EIF_SIGNATURE[i] != header.signature[i]){
            throw runtime_error("EIF wrong signature");
        }
    }

    /* check type */
    if(EIF_TYPE_MULTICOLOR != header.type){
        throw runtime_error("EIF wrong type");
    }

    auto data_offset = sizeof(EifBaseHeader) + EIF_MULTICOLOR_PALETTE_SIZE;
    if(header.length > (data.size() - data_offset)){
        throw runtime_error("EIF wrong data length");
    }

    //get aligned width
    auto aligned16_width = header.length / header.height;
    if(aligned16_width % sizeof(uint16_t)){
        throw runtime_error("EIF not aligned width");
    }

    if((aligned16_width / sizeof(uint16_t)) < header.width){
        throw runtime_error("EIF wrong actual width");
    }

    bitmap_data.clear();
    bitmap_data.reserve(header.height * header.width);
    for (auto i =data_offset; i < data.size(); i+=aligned16_width) {
        bitmap_data.insert(end(bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width * sizeof(uint16_t));
    }

    if(palette.empty()) {
        palette.resize(EIF_MULTICOLOR_PALETTE_SIZE);
        copy(
                data.begin() + sizeof(EifBaseHeader),
                data.begin() + sizeof(EifBaseHeader) + EIF_MULTICOLOR_PALETTE_SIZE,
                palette.begin());
    }

    width = header.width;
    height = header.height;

    return 0;
}

uint8_t EifImage16bit::searchPixel(RGBApixel rgb_pixel) {

    double color_distance = numeric_limits<double>::max();
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

int EifImage16bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.string().c_str());

    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();
    auto num_pixels = height * width;

    bitmap_data.clear();
    bitmap_data.resize(num_pixels * 2);

    if(palette.empty()) {
        vector<uint8_t> pImage_data;
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
        vector<uint8_t> pOut(num_pixels);

        exq_data *pExq;
        pExq = exq_init();
        exq_no_transparency(pExq);
        exq_feed(pExq, (unsigned char *)pImage_data.data(), num_pixels);
        exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
        exq_get_palette(pExq, pPalette, EIF_MULTICOLOR_NUM_COLORS);
        exq_map_image(pExq, num_pixels, (unsigned char *)pImage_data.data(), pOut.data());
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
                auto pixel_idx = i*width+j;
                bitmap_data[0 + 2*pixel_idx] = pOut[pixel_idx];
                bitmap_data[1 + 2*pixel_idx] = pImage_data[pixel_idx*4+3];
            }
        }

    } else {
        cerr << "Use palette form file: " << file_name << endl;
        for(auto i =0; i < height; i++){
            for(auto j=0; j < width; j++){
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
    }

    return 0;
}

void EifImage16bit::saveBmp(const fs::path& file_name) {

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

    bmp_image.WriteToFile(file_name.string().c_str());
}

void EifImage16bit::getBitmap(vector<uint8_t> &data) {

    data.resize(0);
    data.reserve(height * width * 4);
    for(auto i =0; i < height; i++){
        for(auto j=0; j < width; j++){

            auto color_idx = *(bitmap_data.data() + (i * width + j) * 2 + 0);
            uint8_t A = *(bitmap_data.data() + (i * width + j) * 2 + 1);
            uint8_t R = palette[color_idx * 3 + 0];
            uint8_t G = palette[color_idx * 3 + 1];
            uint8_t B = palette[color_idx * 3 + 2];

            data.push_back(R);
            data.push_back(G);
            data.push_back(B);
            data.push_back(A);

        }
    }
}

int EifImage16bit::setPalette(const vector<uint8_t> &data) {

    if(data.size() != EIF_MULTICOLOR_PALETTE_SIZE) {
        cerr << "Error: palette wrong size" << endl;
        return -1;
    }

    palette = data;

    return 0;
}

int EifImage16bit::changePalette(const vector<uint8_t> &data) {

    if(data.size() != EIF_MULTICOLOR_PALETTE_SIZE) {
        cerr << "Error: palette wrong size" << endl;
        return -1;
    }

    palette = data;
    vector<uint8_t> pPalette(EIF_MULTICOLOR_NUM_COLORS*4);
    for(int i =0; i < EIF_MULTICOLOR_NUM_COLORS; ++i) {
        pPalette[i*4+0] = data[i*3+0];
        pPalette[i*4+1] = data[i*3+1];
        pPalette[i*4+2] = data[i*3+2];
        pPalette[i*4+3] = 0;
    }

    auto num_pixels = (int)(width * height);
    vector<uint8_t> mapped_data(num_pixels);
    vector<uint8_t> bitmap;
    getBitmap(bitmap);

    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);
    exq_set_palette(pExq, pPalette.data(), EIF_MULTICOLOR_NUM_COLORS);
    exq_map_image(pExq, num_pixels, (unsigned char *)bitmap.data(), mapped_data.data());
    exq_free(pExq);

    for(int i =0; i < num_pixels; ++i) {
        bitmap_data[i * 2] = mapped_data[i];
    }

    return 0;
}

void EifImage16bit::savePalette(const fs::path& file_name) {

    if(palette.size() != EIF_MULTICOLOR_PALETTE_SIZE) {
        cerr << "Error: palette wrong size" << endl;
        return;
    }

    FTUtils::bufferToFile(file_name, (char *)palette.data(), EIF_MULTICOLOR_PALETTE_SIZE);
}

int EifImage16bit::setBitmap(const vector <uint8_t> &data)
{
    bitmap_data = data;

    return 0;
}

int EifImage32bit::openEif(const vector<uint8_t> &data) {

    if(data.size() < sizeof(EifBaseHeader)){
        throw runtime_error("EIF wrong header length");
    }
    const EifBaseHeader &header = *reinterpret_cast<const struct EifBaseHeader*>(&data[0]);

    /* check signature */
    for(auto i=0; i < sizeof(header.signature); i++){
        if(EIF_SIGNATURE[i] != header.signature[i]){
            throw runtime_error("EIF wrong signature");
        }
    }

    /* check type */
    if(EIF_TYPE_SUPERCOLOR != header.type){
        throw runtime_error("EIF wrong type");
    }

    auto data_offset = sizeof(EifBaseHeader);
    if(header.length > (data.size() - data_offset)){
        throw runtime_error("EIF wrong data length");
    }

    //get aligned width
    auto aligned4_width = header.length / header.height;
    if(aligned4_width % sizeof(uint32_t)){
        throw runtime_error("EIF not aligned width");
    }

    if((aligned4_width / sizeof(uint32_t)) < header.width){
        throw runtime_error("EIF wrong actual width");
    }

    bitmap_data.clear();
    bitmap_data.reserve(header.height * header.width);
    for (auto i =data_offset; i < data.size(); i+=aligned4_width) {

        bitmap_data.insert(end(bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width * sizeof(uint32_t));
    }

    width = header.width;
    height = header.height;

    return 0;
}

void EifImage32bit::saveBmp(const fs::path& file_name) {

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
    bmp_image.WriteToFile(file_name.string().c_str());
}

int EifImage32bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.string().c_str());

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

void EifConverter::eifToBmpFile(const vector<uint8_t>& data, const fs::path& out_file_name,
        const fs::path& palette_file_name)
{

    EifImageBase* image;

    if(data[7] == EIF_TYPE_MONOCHROME) {
        image = new EifImage8bit;
    } else if(data[7] == EIF_TYPE_MULTICOLOR) {
        image = new EifImage16bit;
        if(!palette_file_name.empty()) {
            auto palette = FTUtils::fileToVector(palette_file_name);
            dynamic_cast<EifImage16bit*>(image)->setPalette(palette);
        }
    } else if(data[7] == EIF_TYPE_SUPERCOLOR){
        image = new EifImage32bit;
    } else {
        throw runtime_error("Unsupported EIF format");
    }

    image->openEif(data);
    image->saveBmp(out_file_name);
    if(data[7] == EIF_TYPE_MULTICOLOR) {
        auto pal_path(out_file_name);
        dynamic_cast<EifImage16bit*>(image)->savePalette(pal_path.replace_extension(".pal"));
    }

    delete image;
}

void EifConverter::bmpFileToEifFile(const fs::path& file_name, uint8_t depth, const fs::path& out_file_name,const fs::path& palette_file_name)
{

    EifImageBase* image;
    switch (depth) {
        case 8:
            image = new EifImage8bit;
            break;
        case 16: {
            image = new EifImage16bit;
            if(!palette_file_name.empty()) {
                auto palette = FTUtils::fileToVector(palette_file_name);
                dynamic_cast<EifImage16bit*>(image)->setPalette(palette);
            }
        }
            break;
        case 32:
            image = new EifImage32bit;
            break;
        default:
            throw runtime_error("Incorrect depth value");
    }

    image->openBmp(file_name);
    image->saveEif(out_file_name);

    delete image;
}

int EifConverter::createMultipaletteEifs(const fs::path& bmp_dir, const fs::path& out_dir) {

    vector<fs::path> bmp_files;
    for(auto& p: fs::recursive_directory_iterator(bmp_dir))
    {
        if(p.path().extension() == ".bmp")
            bmp_files.push_back(p.path());
    }
    int f_count = bmp_files.size();

    struct _img {
        vector<uint8_t> bmp_data;
        unsigned w,h;
    };
    vector<_img> img_vec(f_count);

    unsigned char pPalette[EIF_MULTICOLOR_NUM_COLORS * 4] = {};

    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);

    // foreach file
    for(int k=0; k < f_count; ++k) {

        BMP bmp_image;
        bmp_image.ReadFromFile(bmp_files[k].string().c_str());

        auto width = (unsigned)bmp_image.TellWidth();
        auto height = (unsigned)bmp_image.TellHeight();
        auto num_pixels = height * width;

        img_vec[k].w =width;
        img_vec[k].h =height;
        auto& pImage_data = img_vec[k].bmp_data;

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
        exq_feed(pExq, (unsigned char *)pImage_data.data(), num_pixels);
    }

    exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
    exq_get_palette(pExq, pPalette, EIF_MULTICOLOR_NUM_COLORS);

    //convert palette
    vector<uint8_t> eif_palette;
    eif_palette.resize(EIF_MULTICOLOR_NUM_COLORS * 3);
    for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; i++) {
        eif_palette[i * 3 + 0] = pPalette[i * 4 + 0];
        eif_palette[i * 3 + 1] = pPalette[i * 4 + 1];
        eif_palette[i * 3 + 2] = pPalette[i * 4 + 2];
    }

    // foreach file create eif
    vector<uint8_t> mapped_data;
    for(int f=0; f < f_count; ++f) {
        auto& img = img_vec[f];
        auto num_pixels = img.w * img.h;
        mapped_data.resize(num_pixels);
        exq_map_image(pExq, num_pixels, (unsigned char *)img.bmp_data.data(), mapped_data.data());

        vector<uint8_t> bitmap_data(num_pixels*2);
        for(auto i=0; i < img.h; i++){
            for(auto j=0; j < img.w; j++){
                auto pixel_idx = i * img.w + j;
                bitmap_data[pixel_idx * 2] = mapped_data[pixel_idx];
                bitmap_data[pixel_idx * 2 + 1] = img.bmp_data[pixel_idx * 4 + 3];
            }
        }

        EifImage16bit eif(img.w, img.h, eif_palette, bitmap_data);
        eif.saveEif((out_dir / (bmp_files[f].stem().string() + ".eif")).string());
    }

    exq_free(pExq);

    return 0;
}

int EifConverter::createMultipaletteEifs(const vector<EifImage16bit*>& eifs) {

    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);
    unsigned char pPalette[EIF_MULTICOLOR_NUM_COLORS * 4] = {};

    vector<vector<uint8_t>> eifs_bitmaps(eifs.size());

    for (int i =0; i < eifs.size(); ++i) {
        eifs[i]->getBitmap(eifs_bitmaps[i]);
        exq_feed(pExq, (unsigned char *)eifs_bitmaps[i].data(), (int)eifs_bitmaps[i].size()/4);
    }

    exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
    exq_get_palette(pExq, pPalette, EIF_MULTICOLOR_NUM_COLORS);

    //convert palette
    vector<uint8_t> eif_palette;
    eif_palette.resize(EIF_MULTICOLOR_PALETTE_SIZE);
    for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; i++) {
        eif_palette[i * 3 + 0] = pPalette[i * 4 + 0];
        eif_palette[i * 3 + 1] = pPalette[i * 4 + 1];
        eif_palette[i * 3 + 2] = pPalette[i * 4 + 2];
    }

    vector<uint8_t> mapped_data;
    for (int i =0; i < eifs.size(); ++i) {

        auto num_pixels = eifs_bitmaps[i].size()/4;
        mapped_data.resize(num_pixels);
        exq_map_image(pExq, num_pixels, (unsigned char *)eifs_bitmaps[i].data(), mapped_data.data());

        vector<uint8_t> bitmap_data(num_pixels * 2);
        for(int pixel_idx =0; pixel_idx < num_pixels; ++ pixel_idx) {
            bitmap_data[pixel_idx * 2] = mapped_data[pixel_idx];
            bitmap_data[pixel_idx * 2 + 1] = eifs_bitmaps[i][pixel_idx * 4 + 3];
        }

        eifs[i]->setPalette(eif_palette);
        eifs[i]->setBitmap(bitmap_data);
    }

    exq_free(pExq);

    return 0;
}