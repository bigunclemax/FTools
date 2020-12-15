//
// Created by user on 06.12.2020.
//

#include <iterator>
#include <csv.h>
#include "TextSectionPacker.h"

constexpr char LANG_ID[]        = "lang_id";
constexpr char LINE_ID[]        = "line_id";
constexpr char LINE_CONTENT[]   = "line_content";


void TextSectionPacker::unpack(const vector<uint8_t> &bin_data, const fs::path &out_path) {

    auto header_path   = out_path / "header.bin";
    auto alerts_path   = out_path / "ui_alerts.csv";
    auto texts_path    = out_path / "ui_texts.csv";
    auto unknown_path  = out_path / "unknown.bin";
    auto unknown2_path = out_path / "unknown2.bin";
    auto eif_path      = out_path / "eif.bin";

    auto text_section = reinterpret_cast<const TextSection_mk3_5 *>(bin_data.data());

    /** header **/
    // FTUtils::bufferToFile(header_path, text_section->header, sizeof(text_section->header));

    /** TextUI **/
    std::ofstream ui_texts (texts_path, std::ios::binary);
    ui_texts << LANG_ID << "," << LINE_ID << "," << LINE_CONTENT << std::endl;
    int lang_id = 0;
    for(auto pack: text_section->ui_text_pack) {
        int line_id = 0;
        for(auto line: pack.lines) {
            ui_texts << lang_id << "," << line_id++ << ",\""
            << Escape{string(line.line, line.line + sizeof(line.line))}
            << "\"" << std::endl;
        }
        lang_id++;
    }

    /** unk **/
    // FTUtils::bufferToFile(unknown_path, text_section->unk, sizeof(text_section->unk));

    /** TextAlerts **/
    std::ofstream ui_alerts (alerts_path);
    ui_alerts << LANG_ID << "," << LINE_ID << "," << LINE_CONTENT << std::endl;
    lang_id = 0;
    for(auto pack: text_section->alerts_text_pack) {
        for(auto line: pack.lines) {
            ui_alerts << lang_id << "," << line.idx << ",\""
            << Escape{string(line.line, line.line + sizeof(line.line))}
            << "\"" << std::endl;
        }
        lang_id++;
    }

    /** unk2 **/
    // FTUtils::bufferToFile(unknown2_path, text_section->unk2, sizeof(text_section->unk2));
}

void TextSectionPacker::pack(const fs::path &vbf_path, const fs::path &out_path) {

    auto alerts_path   = vbf_path.parent_path() / "ui_alerts.csv";
    auto ui_path    = vbf_path.parent_path() / "ui_texts.csv";

    auto vbf_bin = FTUtils::fileToVector(vbf_path);
    auto text_section = reinterpret_cast<TextSection_mk3_5 *>(vbf_bin.data());

    //pack alerts
    io::CSVReader<3, io::trim_chars<' ', '\t'>, io::double_quote_escape<',','\"'>> alerts_csv(alerts_path.string());
    alerts_csv.read_header(io::ignore_extra_column, LANG_ID, LINE_ID, LINE_CONTENT);

    uint32_t lang_id, line_id;
    std::string line_content;

    auto alert_line_sz = sizeof(text_section->alerts_text_pack->lines->line);
    while(alerts_csv.read_row(lang_id, line_id, line_content)) {

        auto unescaped_str = Unescape(line_content);

        text_section->alerts_text_pack[lang_id].lines[line_id].idx = line_id;
        char* line_ptr = text_section->alerts_text_pack[lang_id].lines[line_id].line;
        auto copy_sz = std::min(unescaped_str.size(), alert_line_sz);
        std::copy(unescaped_str.begin(), unescaped_str.begin() + copy_sz, line_ptr);
        std::fill(line_ptr + copy_sz, line_ptr + alert_line_sz, '\0');
    }

    //pack ui texts
    io::CSVReader<3, io::trim_chars<' ', '\t'>, io::double_quote_escape<',','\"'>> ui_csv(ui_path.string());
    ui_csv.read_header(io::ignore_extra_column, LANG_ID, LINE_ID, LINE_CONTENT);

    auto ui_line_sz = sizeof(text_section->ui_text_pack->lines->line);
    while(ui_csv.read_row(lang_id, line_id, line_content)) {

        auto unescaped_str = Unescape(line_content);

        char* line_ptr = text_section->ui_text_pack[lang_id].lines[line_id].line;
        auto copy_sz = std::min(unescaped_str.size(), ui_line_sz);
        std::copy(unescaped_str.begin(), unescaped_str.begin() + copy_sz, line_ptr);
        std::fill(line_ptr + copy_sz, line_ptr + ui_line_sz, '\0');
    }

    // save to out file
    FTUtils::vectorToFile(out_path, vbf_bin);
}
