//
// Created by bigun on 12/28/2018.
//

#include "EifConverter.h"
#include <fstream>
#include <utils.h>
#include <exoquant.h>
#include <algorithm>
#include <list>

using namespace std;
using namespace EIF;

vector<uint8_t> EifImageBase::saveEifToVector() const {

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
    header.length = (header.type == EIF_TYPE::EIF_TYPE_MULTICOLOR)? width * height * sizeof(uint16_t) : m_bitmap_data.size();

    /* set palette */
    store_palette(data);

    /* set pixels */
    store_bitmap(data);

    return data;
}

void EifImageBase::saveEif(const fs::path& file_name) const {
    FTUtils::vectorToFile(file_name,saveEifToVector());
}

void EifImage8bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    if (!bmp_image.ReadFromFile(FTUtils::path2c_str(file_name).c_str())) {
        throw runtime_error("Error open bmp '" + file_name.string() + "'");
    }

    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();

    auto aligned_width = (width % 4) ? (width/4 + 1)*4 : width;
    m_bitmap_data.clear();
    m_bitmap_data.resize(height * aligned_width);

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < aligned_width; ++i){
            uint8_t pixel;
            if(i >= width) {
                pixel = 0x0;
            } else {
                RGBApixel rgb_pixel = bmp_image.GetPixel(i, j);
                pixel = (uint8_t)((rgb_pixel.Red + rgb_pixel.Green + rgb_pixel.Blue)/3);
            }
            m_bitmap_data[j * aligned_width + i] = pixel;
        }
    }
}

void EifImage8bit::saveBmp(const fs::path& file_name) const {

    BMP bmp_image;
    if (!bmp_image.SetSize((int)width, (int)height)) {
        throw runtime_error("Error set size of bmp");
    }

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){
            auto color = *(&m_bitmap_data[0] + j * width + i);

            RGBApixel rgb_pixel;
            rgb_pixel.Alpha = 0xFF;
            rgb_pixel.Red = color;
            rgb_pixel.Green = color;
            rgb_pixel.Blue = color;

            bmp_image.SetPixel(i, j, rgb_pixel);
        }
    }

    if (!bmp_image.WriteToFile(FTUtils::path2c_str(file_name).c_str())) {
        throw runtime_error("Error save bmp '" + file_name.string() + "'");
    }
}

vector<uint8_t> EifImage8bit::getBitmapRBGA() const {

    vector<uint8_t> bitmap(m_bitmap_data.size() * 4);
    for (int i = 0; i < m_bitmap_data.size(); ++i) {
        bitmap[i * 4 + 0] = m_bitmap_data[i];
        bitmap[i * 4 + 1] = m_bitmap_data[i];
        bitmap[i * 4 + 2] = m_bitmap_data[i];
        bitmap[i * 4 + 3] = 0xFF; // no transparency for 8bit eif
    }
    return bitmap;
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

    m_bitmap_data.clear();
    m_bitmap_data.reserve(header.height * header.width);
    for (unsigned i = data_offset; i < data.size(); i+=aligned4_width) {

        m_bitmap_data.insert(end(m_bitmap_data),
                           data.begin() + i,
                           data.begin() + i + header.width);
    }

    width = header.width;
    height = header.height;

    return 0;
}

void EifImage16bit::store_palette(vector<uint8_t> &data) const {

    if (m_palette.size() != EIF_MULTICOLOR_NUM_COLORS * 4) {
        throw runtime_error("Palette has wrong size");
    }

    for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; ++i) {
        data.push_back(m_palette[i*4+0]);
        data.push_back(m_palette[i*4+1]);
        data.push_back(m_palette[i*4+2]);
    }
}

void EifImage16bit::store_bitmap(vector<uint8_t> &data) const {

    auto num_pixels = width * height;
    vector<uint8_t> mapped_data(num_pixels);

    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);
    exq_set_palette(pExq, const_cast<unsigned char *>(m_palette.data()), EIF_MULTICOLOR_NUM_COLORS);
    exq_map_image(pExq, (int)num_pixels, const_cast<unsigned char *>(m_bitmap_data.data()), mapped_data.data());
    exq_free(pExq);

    for(int i=0; i < num_pixels; ++i) {
        data.push_back(mapped_data[i]);
        data.push_back(m_bitmap_data[i * 4 + 3]);
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
    if((header.length / header.height) % sizeof(uint16_t)){
        throw runtime_error("EIF not aligned width");
    }

    if(m_palette.empty()) {
        m_palette.resize(EIF_MULTICOLOR_NUM_COLORS * 4);
        for(int i=0; i < EIF_MULTICOLOR_NUM_COLORS; ++i) {
            m_palette[i*4+0] = data[sizeof(EifBaseHeader)+i*3+0];
            m_palette[i*4+1] = data[sizeof(EifBaseHeader)+i*3+1];
            m_palette[i*4+2] = data[sizeof(EifBaseHeader)+i*3+2];
            m_palette[i*4+3] = 0;
        }
    }

    //RGBA
    m_bitmap_data.resize(header.height * header.width * 4);

    for (auto i = 0; i < (data.size() - data_offset)/2; ++i) {
        m_bitmap_data[i*4+0] = m_palette[data[data_offset + i*2]*4 + 0];
        m_bitmap_data[i*4+1] = m_palette[data[data_offset + i*2]*4 + 1];
        m_bitmap_data[i*4+2] = m_palette[data[data_offset + i*2]*4 + 2];
        m_bitmap_data[i*4+3] = data[data_offset + i*2 + 1];
    }

    width = header.width;
    height = header.height;

    return 0;
}

void EifImage16bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    if (!bmp_image.ReadFromFile(FTUtils::path2c_str(file_name).c_str())) {
        throw runtime_error("Error open bmp '" + file_name.string() + "'");
    }

    bool hasAlpha = (bmp_image.TellBitDepth() == 32);

    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();
    m_bitmap_data.resize(height * width * 4);

    for(auto j =0; j < height; j++){
        for(auto i=0; i < width; i++){
            RGBApixel rgb_pixel = bmp_image.GetPixel(i, j);
            m_bitmap_data[(width*j+i)*4+0] = rgb_pixel.Red;
            m_bitmap_data[(width*j+i)*4+1] = rgb_pixel.Green;
            m_bitmap_data[(width*j+i)*4+2] = rgb_pixel.Blue;
            m_bitmap_data[(width*j+i)*4+3] = hasAlpha ? rgb_pixel.Alpha : 0xFF;
        }
    }

    if(m_palette.empty()) {

        //generated palette
        exq_data *pExq = exq_init();
        exq_no_transparency(pExq);
        exq_feed(pExq, (unsigned char *) m_bitmap_data.data(), (int)(height * width));
        exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
        m_palette.resize(EIF_MULTICOLOR_NUM_COLORS * 4);
        exq_get_palette(pExq, m_palette.data(), EIF_MULTICOLOR_NUM_COLORS);
        exq_free(pExq);
    }
}

void EifImage16bit::saveBmp(const fs::path& file_name) const {

    BMP bmp_image;
    bmp_image.SetBitDepth(32);
    if (!bmp_image.SetSize((int)width, (int)height)) {
        throw runtime_error("Error set size of bmp");
    }

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){
            RGBApixel rgb_pixel;
            rgb_pixel.Alpha = m_bitmap_data[(width*j+i)*4+3];
            rgb_pixel.Red   = m_bitmap_data[(width*j+i)*4+0];
            rgb_pixel.Green = m_bitmap_data[(width*j+i)*4+1];
            rgb_pixel.Blue  = m_bitmap_data[(width*j+i)*4+2];

            bmp_image.SetPixel(i, j, rgb_pixel);
        }
    }

    if (!bmp_image.WriteToFile(FTUtils::path2c_str(file_name).c_str())) {
        throw runtime_error("Error save bmp '" + file_name.string() + "'");
    }
}

int EifImage16bit::setPalette(const vector<uint8_t> &data) {

    if(data.size() != EIF_MULTICOLOR_NUM_COLORS*4) {
        cerr << "Error: palette wrong size" << endl;
        return -1;
    }

    m_palette = data;

    return 0;
}

void EifImage16bit::savePalette(const fs::path& file_name) const {

    if(m_palette.empty()) {
        cerr << "Error: palette wrong size" << endl;
        return;
    }

    FTUtils::vectorToFile(file_name, m_palette);
}

vector<uint8_t> EifImage16bit::getBitmapRBGA() const {
    return m_bitmap_data;
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

    m_bitmap_data.clear();
    m_bitmap_data.reserve(header.height * header.width);
    for (unsigned i = data_offset; i < data.size(); i+=aligned4_width) {

        m_bitmap_data.insert(end(m_bitmap_data),
                           data.begin() + i,
                           data.begin() + i + (unsigned)(header.width * sizeof(uint32_t)));
    }

    width = header.width;
    height = header.height;

    return 0;
}

void EifImage32bit::saveBmp(const fs::path& file_name) const {

    BMP bmp_image;
    bmp_image.SetBitDepth(32);
    if (!bmp_image.SetSize((int)width, (int)height)) {
        throw runtime_error("Error set size of bmp");
    }

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){

            RGBApixel rgb_pixel;

            rgb_pixel.Alpha = *(&m_bitmap_data[0] + 3 + j * width * 4 + i * 4);
            rgb_pixel.Red   = *(&m_bitmap_data[0] + 2 + j * width * 4 + i * 4);
            rgb_pixel.Green = *(&m_bitmap_data[0] + 1 + j * width * 4 + i * 4);
            rgb_pixel.Blue  = *(&m_bitmap_data[0] + 0 + j * width * 4 + i * 4);

            bmp_image.SetPixel(i, j, rgb_pixel);
        }
    }
    if (!bmp_image.WriteToFile(FTUtils::path2c_str(file_name).c_str())) {
        throw runtime_error("Error save bmp '" + file_name.string() + "'");
    }
}

vector<uint8_t> EifImage32bit::getBitmapRBGA() const {

    vector<uint8_t> bitmap(m_bitmap_data.size() * 4);
    /* swap BGRA to RGBA */
    for (int i = 0; i < m_bitmap_data.size(); i += 4) {
        bitmap[i + 0] = m_bitmap_data[i + 2];
        bitmap[i + 1] = m_bitmap_data[i + 1];
        bitmap[i + 2] = m_bitmap_data[i + 0];
        bitmap[i + 3] = m_bitmap_data[i + 3];
    }
    return bitmap;
}

void EifImage32bit::openBmp(const fs::path& file_name) {

    BMP bmp_image;
    if (!bmp_image.ReadFromFile(FTUtils::path2c_str(file_name).c_str())) {
        throw runtime_error("Error open bmp '" + file_name.string() + "'");
    }

    bool hasAlpha = (bmp_image.TellBitDepth() == 32);

    width = (unsigned)bmp_image.TellWidth();
    height = (unsigned)bmp_image.TellHeight();

    m_bitmap_data.clear();
    m_bitmap_data.resize(height * width * 4);

    for(auto j =0; j < height; ++j){
        for(auto i=0; i < width; ++i){

            RGBApixel rgb_pixel = bmp_image.GetPixel(i, j);

            m_bitmap_data[0 + 3 + j * width * 4 + i * 4] = hasAlpha ? rgb_pixel.Alpha : 0xFF;
            m_bitmap_data[0 + 2 + j * width * 4 + i * 4] = rgb_pixel.Red;
            m_bitmap_data[0 + 1 + j * width * 4 + i * 4] = rgb_pixel.Green;
            m_bitmap_data[0 + 0 + j * width * 4 + i * 4] = rgb_pixel.Blue;
        }
    }
}

void EifConverter::eifToBmpFile(const vector<uint8_t> &data, const fs::path &out_file_name,
                                const fs::path &palette_file_name, bool store_palette)
{

    auto image = makeEif((EIF_TYPE) data[7]);

    if(!palette_file_name.empty() && image->getType() == EIF_TYPE_MULTICOLOR) {
        auto palette = FTUtils::fileToVector(palette_file_name);
        image->setPalette(palette);
    }

    image->openEif(data);
    image->saveBmp(out_file_name);
    if (image->getType() == EIF_TYPE_MULTICOLOR && store_palette) {
        auto pal_path(out_file_name);
        image->savePalette(pal_path.replace_extension(".pal"));
    }
}

void EifConverter::bmpFileToEifFile(const fs::path& file_name, uint8_t depth, const fs::path& out_file_name,const fs::path& palette_file_name)
{

    auto image = makeEif(depthToEifType(depth));

    if(!palette_file_name.empty()) {
        auto palette = FTUtils::fileToVector(palette_file_name);
        image->setPalette(palette);
    }

    image->openBmp(file_name);
    image->saveEif(out_file_name);
}

void EifConverter::mapMultiPalette(vector<EifImage16bit>& eifs)
{
    exq_data *pExq = exq_init();
    exq_no_transparency(pExq);

    for (const auto& eif :eifs) {
        exq_feed(pExq, (unsigned char *) eif.getBitmapRBGA().data(), (int) eif.getBitmapRBGA().size() / 4);
    }

    exq_quantize(pExq, EIF_MULTICOLOR_NUM_COLORS);
    vector<uint8_t> paletteRGBA(EIF_MULTICOLOR_NUM_COLORS*4);
    exq_get_palette(pExq, paletteRGBA.data(), EIF_MULTICOLOR_NUM_COLORS);

    for (auto& eif :eifs) {
        eif.setPalette(paletteRGBA);
    }

    exq_free(pExq);
}

int EifConverter::bulkPack(const fs::path& bmp_dir, const fs::path& out_dir) {

    vector<fs::path> bmp_files;
    for(auto& p: fs::recursive_directory_iterator(bmp_dir))
    {
        if(p.path().extension() == ".bmp")
            bmp_files.push_back(p.path());
    }
    auto f_count = bmp_files.size();

    vector<EifImage16bit> eifs(f_count);

    for(auto f=0; f < f_count; ++f) {
        eifs[f].openBmp(bmp_files[f]);
    }

    mapMultiPalette(eifs);

    for(auto f=0; f < f_count; ++f) {
        eifs[f].saveEif((out_dir / (bmp_files[f].stem().string() + ".eif")).string());
    }

    return 0;
}

unique_ptr<EifImageBase> EifConverter::makeEif(EIF_TYPE type) {

    unique_ptr<EifImageBase> image;

    if (type == EIF_TYPE_MONOCHROME) {
        image = make_unique<EifImage8bit>();
    } else if (type == EIF_TYPE_MULTICOLOR) {
        image = make_unique<EifImage16bit>();
    } else if (type == EIF_TYPE_SUPERCOLOR) {
        image = make_unique<EifImage32bit>();
    } else {
        throw runtime_error("Unsupported EIF format");
    }

    return image;
}