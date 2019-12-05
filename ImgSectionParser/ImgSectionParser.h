//
// Created by bigun on 22.01.2019.
//

#ifndef FORDTOOLS_IMGSECTIONPARSER_H
#define FORDTOOLS_IMGSECTIONPARSER_H

#include <vector>
#include <array>

struct StrangeItem {
    std::array<uint8_t, 24> data;
};

struct ZipItemHead {
    uint32_t a;
    uint32_t b;
    std::array<uint8_t, 32> fileName;
};

struct TtfItemHead {
    std::array<uint8_t, 32> fileName;
};

class ImgSectionParser {
    std::vector<StrangeItem> m_strange_items;
    std::vector<ZipItemHead> m_zip_items;
    std::vector<TtfItemHead> m_ttf_items;

public:
    void parseSection(uint8_t* data, unsigned data_len);
};


#endif //FORDTOOLS_IMGSECTIONPARSER_H
