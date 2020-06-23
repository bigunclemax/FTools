//
// Created by bigun on 12/28/2018.
//

#include "EifConverter.h"
#include <fstream>
#include <limits>
#include <utils.h>
#include <exoquant.h>
#include <algorithm>
#include <list>

using namespace EIF;

vector<uint8_t> EifImageBase::saveEifToVector() {

    vector<uint8_t> data;

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

    return data;
}

void EifImageBase::saveEif(const fs::path& file_name) {
    FTUtils::vectorToFile(file_name,saveEifToVector());
}

int EifImage8bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    bmp_image.ReadFromFile(file_name.string().c_str());

    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();

    auto aligned_width = (width % 4) ? (width/4 + 1)*4 : width;
    bitmap_data.clear();
    bitmap_data.resize(height * aligned_width);

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < aligned_width; ++i){
            uint8_t pixel;
            if(i >= width) {
                pixel = 0x0;
            } else {
                RGBApixel rgb_pixel = bmp_image.GetPixel(i, j);
                pixel = (uint8_t)((rgb_pixel.Red + rgb_pixel.Green + rgb_pixel.Blue)/3);
            }
            bitmap_data[j * aligned_width + i] = pixel;
        }
    }

    return 0;
}

void EifImage8bit::saveBmp(const fs::path& file_name){

    BMP bmp_image;
    bmp_image.SetSize((int)width, (int)height);

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){
            auto color = *(&bitmap_data[0] + j * width + i);

            RGBApixel rgb_pixel;
            rgb_pixel.Alpha = 0xFF;
            rgb_pixel.Red = color;
            rgb_pixel.Green = color;
            rgb_pixel.Blue = color;

            bmp_image.SetPixel(i, j, rgb_pixel);
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
                             const vector<uint8_t> &bitmap, const vector<uint8_t> &alpha)
{
    bool no_alpha = alpha.empty();

    if(!no_alpha && alpha.size() != bitmap.size())
        throw runtime_error("Bitmap and alpha data size mismatch");

    width = w;
    height = h;
    palette = pal;

    bitmap_data.resize(bitmap.size()*2);
    for(int i=0; i < bitmap.size(); ++i) {
        bitmap_data[i*2] = bitmap[i];
        bitmap_data[i*2+1] = no_alpha ? 0xFF : alpha[i];
    }
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

    if(palette.empty()) {
        //generated palette
        auto bitmap = EifConverter::bmpToBitmap(file_name.string().c_str());
        width = bitmap.width;
        height = bitmap.height;
        auto num_pixels = height * width;

        vector<uint8_t> mapped(num_pixels);
        vector<uint8_t> pPalette(EIF_MULTICOLOR_NUM_COLORS * 4);

        exq_data * pExq = exq_init();
        exq_no_transparency(pExq);
        exq_feed(pExq, (unsigned char *)bitmap.bitmapRGBA.data(), num_pixels);
        exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
        exq_get_palette(pExq, pPalette.data(), EIF_MULTICOLOR_NUM_COLORS);
        exq_map_image(pExq, num_pixels, (unsigned char *)bitmap.bitmapRGBA.data(), mapped.data());
        exq_free(pExq);

        //convert palette
        palette.resize(EIF_MULTICOLOR_NUM_COLORS * 3);
        for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; i++) {
            palette[i*3+0] = pPalette[i*4+0];
            palette[i*3+1] = pPalette[i*4+1];
            palette[i*3+2] = pPalette[i*4+2];
        }

        for(auto i =0; i < num_pixels; ++i){
            bitmap_data[i*2+0] = mapped[i];
            bitmap_data[i*2+1] = bitmap.bitmapAlpha[i];
        }

    } else {
        // palette from file
        BMP bmp_image;
        bmp_image.ReadFromFile(file_name.string().c_str());

        width = (unsigned)bmp_image.TellWidth();
        height = (unsigned)bmp_image.TellHeight();
        bitmap_data.resize(height * width * 2);

        for(auto j =0; j < height; j++){
            for(auto i=0; i < width; i++){
                RGBApixel rgb_pixel = bmp_image.GetPixel(i, j);
                bitmap_data[0 + j * width * 2 + i * 2] = searchPixel(rgb_pixel);
                bitmap_data[1 + j * width * 2 + i * 2] = rgb_pixel.Alpha;
            }
        }
    }

    return 0;
}

void EifImage16bit::saveBmp(const fs::path& file_name) {

    BMP bmp_image;
    bmp_image.SetBitDepth(32);
    bmp_image.SetSize((int)width, (int)height);

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){
            RGBApixel rgb_pixel;
            auto color_idx = *(&bitmap_data[0] + j * width * 2 + i * 2);
            rgb_pixel.Alpha = *(&bitmap_data[0] + 1 + j * width * 2 + i * 2);

            rgb_pixel.Red = palette[color_idx*3];
            rgb_pixel.Green = palette[color_idx*3+1];
            rgb_pixel.Blue = palette[color_idx*3+2];

            bmp_image.SetPixel(i, j, rgb_pixel);
        }
    }

    bmp_image.WriteToFile(file_name.string().c_str());
}

BitmapData EifImage16bit::getBitmap() {

    auto num_pixels = height * width;
    vector<uint8_t> data(num_pixels * 4);
    vector<uint8_t> alpha(num_pixels);
    for(auto i =0; i < num_pixels; ++i){
        alpha[i] = bitmap_data[i * 2 + 1];
        auto color_idx = bitmap_data[i * 2];
        data[i * 4 + 0] = palette[color_idx * 3 + 0];
        data[i * 4 + 1] = palette[color_idx * 3 + 1];
        data[i * 4 + 2] = palette[color_idx * 3 + 2];
        data[i * 4 + 3] = 0;
    }
    return {data, alpha, width, height};
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
    auto bitmap = getBitmap();

    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);
    exq_set_palette(pExq, pPalette.data(), EIF_MULTICOLOR_NUM_COLORS);
    exq_map_image(pExq, num_pixels, (unsigned char *)bitmap.bitmapRGBA.data(), mapped_data.data());
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

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){

            RGBApixel rgb_pixel;

            rgb_pixel.Alpha = *(&bitmap_data[0] + 3 + j * width * 4 + i * 4);
            rgb_pixel.Red   = *(&bitmap_data[0] + 2 + j * width * 4 + i * 4);
            rgb_pixel.Green = *(&bitmap_data[0] + 1 + j * width * 4 + i * 4);
            rgb_pixel.Blue  = *(&bitmap_data[0] + 0 + j * width * 4 + i * 4);

            bmp_image.SetPixel(i, j, rgb_pixel);
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

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){

            RGBApixel rgb_pixel = bmp_image.GetPixel(i, j);

            bitmap_data[0 + 3 + j * width * 4 + i * 4] = rgb_pixel.Alpha;
            bitmap_data[0 + 2 + j * width * 4 + i * 4] = rgb_pixel.Red;
            bitmap_data[0 + 1 + j * width * 4 + i * 4] = rgb_pixel.Green;
            bitmap_data[0 + 0 + j * width * 4 + i * 4] = rgb_pixel.Blue;
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

vector<EifImage16bit> EifConverter::mapMultiPalette(const vector<BitmapData>& bitmaps)
{
    vector<EifImage16bit> eifs;

    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);

    for (const auto& bitmap :bitmaps) {
        exq_feed(pExq, (unsigned char *)bitmap.bitmapRGBA.data(), (int)bitmap.bitmapRGBA.size()/4);
    }

    exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
    vector<uint8_t> paletteRGBA(EIF_MULTICOLOR_NUM_COLORS*4);
    exq_get_palette(pExq, paletteRGBA.data(), EIF_MULTICOLOR_NUM_COLORS);

    //convert palette
    vector<uint8_t> eif_palette(EIF_MULTICOLOR_PALETTE_SIZE);
    for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; i++) {
        eif_palette[i * 3 + 0] = paletteRGBA[i * 4 + 0];
        eif_palette[i * 3 + 1] = paletteRGBA[i * 4 + 1];
        eif_palette[i * 3 + 2] = paletteRGBA[i * 4 + 2];
    }

    vector<uint8_t> mapped_data;
    for (const auto& bitmap :bitmaps) {
        mapped_data.resize(bitmap.bitmapRGBA.size()/4);
        exq_map_image(pExq,(int)bitmap.bitmapRGBA.size()/4, (unsigned char *)bitmap.bitmapRGBA.data(),
                mapped_data.data());
        eifs.emplace_back(bitmap.width, bitmap.height, eif_palette, mapped_data, bitmap.bitmapAlpha);
    }

    exq_free(pExq);

    return eifs;
}

int EifConverter::bulkPack(const fs::path& bmp_dir, const fs::path& out_dir) {

    vector<fs::path> bmp_files;
    for(auto& p: fs::recursive_directory_iterator(bmp_dir))
    {
        if(p.path().extension() == ".bmp")
            bmp_files.push_back(p.path());
    }
    int f_count = bmp_files.size();

    vector<BitmapData> bitmaps(f_count);

    for(auto f=0; f < f_count; ++f) {
        bitmaps[f] = bmpToBitmap(bmp_files[f].string().c_str());
    }

    auto eifs = mapMultiPalette(bitmaps);
    for(auto f=0; f < f_count; ++f) {
        eifs[f].saveEif((out_dir / (bmp_files[f].stem().string() + ".eif")).string());
    }

    return 0;
}

BitmapData EifConverter::bmpToBitmap(const fs::path& file) {

    BitmapData data;

    BMP bmp_image;
    bmp_image.ReadFromFile(file.string().c_str());
    data.width = (unsigned)bmp_image.TellWidth();
    data.height = (unsigned)bmp_image.TellHeight();
    auto num_pixels = data.width * data.height;
    data.bitmapRGBA.reserve(num_pixels*4);
    data.bitmapAlpha.reserve(num_pixels);
    for(auto j =0; j < data.height; ++j){
        for(auto i=0; i < data.width; ++i){
            auto px = bmp_image.GetPixel(i, j);
            data.bitmapRGBA.push_back(px.Red);
            data.bitmapRGBA.push_back(px.Green);
            data.bitmapRGBA.push_back(px.Blue);
            data.bitmapRGBA.push_back(0);
            data.bitmapAlpha.push_back(px.Alpha);
        }
    }

    return data;
}