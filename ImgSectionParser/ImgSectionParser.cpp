//
// Created by bigun on 22.01.2019.
//

#include "ImgSectionParser.h"
#include <stdexcept>

void ImgSectionParser::parseSection(uint8_t* data, unsigned data_len) {

    //read strange items
    auto read_offset =0;
    if(data_len < read_offset + sizeof(uint32_t))
        throw std::runtime_error("Incorrect input size");
    unsigned strangeItemsCount = *reinterpret_cast<uint32_t*>(&data[read_offset]);
    read_offset += sizeof(uint32_t);
    if(data_len < (read_offset + strangeItemsCount * sizeof(StrangeItem)))
        throw std::runtime_error("Incorrect input size");

    m_strange_items.clear();
    m_strange_items.reserve(strangeItemsCount);
    for(auto i=0; i < strangeItemsCount; i++) {
        m_strange_items[i] = reinterpret_cast<StrangeItem *>(read_offset)[i];
    }
    read_offset += strangeItemsCount * sizeof(StrangeItem);

    //read zip img files headers
    if(data_len < read_offset + sizeof(uint32_t))
        throw std::runtime_error("Incorrect input size");
    unsigned zipItemsCount = *reinterpret_cast<uint32_t *>(read_offset);
    read_offset += sizeof(uint32_t);
    if(data_len < (read_offset + zipItemsCount * sizeof(ZipItemHead)))
        throw std::runtime_error("Incorrect input size");

    m_zip_items.clear();
    m_zip_items.reserve(zipItemsCount);
    for(auto i=0; i < zipItemsCount; i++) {
        m_zip_items[i] = reinterpret_cast<ZipItemHead *>(read_offset)[i];
    }
    read_offset += zipItemsCount * sizeof(ZipItemHead);

    //read ttf img files headers
    if(data_len < read_offset + sizeof(uint32_t))
        throw std::runtime_error("Incorrect input size");
    unsigned ttfItemsCount = *reinterpret_cast<uint32_t *>(read_offset);
    read_offset += sizeof(uint32_t);
    if(data_len < (read_offset+ zipItemsCount * sizeof(TtfItemHead)))
        throw std::runtime_error("Incorrect input size");

    m_ttf_items.clear();
    m_ttf_items.reserve(ttfItemsCount);
    for(auto i=0; i < ttfItemsCount; i++) {
        m_ttf_items[i] = reinterpret_cast<TtfItemHead *>(read_offset)[i];
    }
    read_offset += ttfItemsCount * sizeof(TtfItemHead);

}