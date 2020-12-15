//
// Created by user on 06.12.2020.
//

#ifndef FORDTOOLS_TEXTSECTIONPACKER_H
#define FORDTOOLS_TEXTSECTIONPACKER_H

#include <cstdint>
#include <vector>
#include <filesystem>
#include <utils.h>
#include <iostream>

using std::vector;
using std::string;

class TextSectionPacker {

    struct Escape
    {
        string str;

        friend inline std::ostream& operator<<(std::ostream& os, const Escape& e)
        {
            for (auto& char_p : e.str)
            {
                switch (char_p)
                {
                    case '\0':  os << "\\0"; break;
                    case '\a':  os << "\\a"; break;
                    case '\b':  os << "\\b"; break;
                    case '\f':  os << "\\f"; break;
                    case '\n':  os << "\\n"; break;
                    case '\r':  os << "\\r"; break;
                    case '\t':  os << "\\t"; break;
                    case '\v':  os << "\\v"; break;
                    case '\\':  os << "\\\\"; break;
                    case '\'':  os << "\\'"; break;
                    case '\"':  os << "\\\""; break;
                    case '\?':  os << "\\\?"; break;
                    default: os << char_p;
                }
            }
            return os;
        }
    };

    static std::string Unescape(const std::string& s) {

        std::string unescaped_str;
        bool is_special = false;

        for (auto char_p : s) {
            if (is_special) {
                is_special = false;
                switch (char_p)
                {
                    case '0':  unescaped_str += '\0'; break;
                    case 'a':  unescaped_str += '\a'; break;
                    case 'b':  unescaped_str += '\b'; break;
                    case 'f':  unescaped_str += '\f'; break;
                    case 'n':  unescaped_str += '\n'; break;
                    case 'r':  unescaped_str += '\r'; break;
                    case 't':  unescaped_str += '\t'; break;
                    case 'v':  unescaped_str += '\v'; break;
                    case '\\': unescaped_str += '\\'; break;
                    case '\'': unescaped_str += '\''; break;
                    case '"':  unescaped_str += '\"'; break;
                    case '?':  unescaped_str += '\?'; break;
                    default: std::cerr << "unknown special character " << char_p << std::endl;
                }
                continue;
            }
            if (char_p == '\\') {
                is_special = true;
                continue;
            }
            unescaped_str += char_p;
        }
        return unescaped_str;
    }

#pragma pack(1)
    struct TextSection_mk3 {

        char header[0x19e2c]; //20 byte lines at 13790 (first uin16_t is index)

        struct TextUI_L {
            char line[0x56];
        };
        struct TextUI {
            TextUI_L lines[0x414];
        } ui_text_pack[19];

        char unk[0x2FA0];

        struct TextAlerts_L {
            uint16_t idx;
            char line[0xC6];
        };
        struct TextAlerts {
//            TextAlerts_L lines[0xe0]; //ACC ACA
            TextAlerts_L lines[0xe3]; //e3 - ACE
        } alerts_text_pack[19];

        char unk2[0x13324A]; //langs list (ace) 0x3c0818

        struct EIF_H {
            char     signature[7];
            char     type;
            uint32_t length;
            uint16_t width;
            uint16_t height;
        } eif_head;
        char eif_content[8000];
    };

    struct TextSection_mk3_5 {

        char header[0x1d680];

        struct TextUI_L {
            char line[0x28];
        };

        struct TextUI {
            TextUI_L lines[0x438];
        } ui_text_pack[18];

        struct TextUI_L_ex {
            char line[0x56];
        };

        struct TextUI_ex {
            TextUI_L_ex lines[0x438];
        } ui_text_pack_ex;

        char unk[0x37ec];

        struct TextAlerts_L {
            uint16_t idx;
            char     line[0xC6];
        };

        struct TextAlerts {
            TextAlerts_L lines[0xf6];
        } alerts_text_pack[19];
    };
#pragma pack()

public:
    static void unpack(const vector<uint8_t> &bin_data, const fs::path &out_path);
    static void pack(const fs::path &in_path, const fs::path &out_path);
};


#endif //FORDTOOLS_TEXTSECTIONPACKER_H
