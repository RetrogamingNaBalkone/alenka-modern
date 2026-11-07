#include "original.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace alenka {
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& path, std::size_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open original resource: " + path.u8string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    if (bytes.size() != size) throw std::runtime_error("Unexpected resource size: " + path.u8string());
    return bytes;
}
unsigned word(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p + 2 > b.size()) throw std::runtime_error("Truncated original resource");
    return b[p] | (unsigned(b[p + 1]) << 8);
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

Rect fit_display(int width, int height) {
    if (width <= 0 || height <= 0) return {0, 0, 0, 0};
    int w = width, h = height;
    if (std::int64_t(width) * 3 >= std::int64_t(height) * 4)
        w = int(std::int64_t(height) * 4 / 3);
    else h = int(std::int64_t(width) * 3 / 4);
    return {(width - w) / 2, (height - h) / 2, w, h};
}

Original::Original(const std::filesystem::path& directory) {
    const auto lib = read(directory / "ALLE.LIB", 93696);
    const auto fld = read(directory / "ALLE.FLD", 43872);
    lessons_ = read(directory / "ALLE.TXT", 9931);
    if (std::filesystem::exists(directory / "ALENKA.DAT")) {
        data_ = read(directory / "ALENKA.DAT", 0x1300);
    } else {
        // Development checks can still read the untouched DOS reference.
        const auto exe = read(directory / "ALLE.EXE", 73999);
        require(word(exe, 0) == 0x5a4d && word(exe, 8) == 32, "Unsupported DOS executable");
        constexpr std::size_t data_offset = 0x200 + 0x8530;
        data_.assign(exe.begin() + data_offset, exe.begin() + data_offset + 0x1300);
    }
    require(word(lib, 0) == 0 && word(lib, 4) == 583 && word(lib, 6) == 39 &&
            word(lib, 8) == 8 && word(lib, 10) == 8 && word(lib, 12) == 40 &&
            word(lib, 14) == 73 && word(lib, 16) == 1, "Unsupported GRED Library header");
    require(std::equal(lib.begin()+120, lib.begin()+132, "GRED Library"), "Invalid GRED Library signature");
    require(word(fld, 0) == 0 && word(fld, 4) == 21920 && word(fld, 6) == 0 &&
            word(fld, 8) == 80 && word(fld, 10) == 274, "Unsupported GRED Field header");
    require(std::equal(fld.begin()+12, fld.begin()+22, "GRED Field"), "Invalid GRED Field signature");
    constexpr unsigned levels[] = {0, 85, 170, 255};
    for (unsigned i = 0; i < palette_.size(); ++i) {
        unsigned rgb[3];
        for (unsigned c = 0; c < 3; ++c) {
            unsigned value = word(lib, 18+i*6+c*2);
            require(value < 4, "Invalid EGA palette component");
            rgb[c] = levels[value];
        }
        palette_[i] = 0xff000000u | (rgb[0]<<16) | (rgb[1]<<8) | rgb[2];
    }
    atlas_.resize(320*584);
    // EXE 1000:527a: planes are stored 3,2,1,0; each row is 40 bytes.
    for (int y = 0; y < 584; ++y) for (int x = 0; x < 320; ++x) {
        unsigned color = 0;
        for (int p = 0; p < 4; ++p)
            color |= ((lib[256+(3-p)*40*584+y*40+x/8] >> (7-x%8)) & 1) << p;
        atlas_[y*320+x] = static_cast<std::uint8_t>(color);
    }
    field_.reserve(80*274);
    for (std::size_t p = 32; p < fld.size(); p += 2) {
        unsigned value = word(fld, p);
        require(value >= 16000, "Invalid GRED tile address");
        unsigned offset = value - 16000;
        require(offset / 40 + 8 <= 584, "GRED tile outside atlas");
        field_.push_back(static_cast<std::uint16_t>(offset));
    }
}

std::uint16_t Original::constant(std::size_t offset) const {
    return static_cast<std::uint16_t>(word(data_, offset));
}
std::uint16_t Original::tile(unsigned index) const {
    require(index < 40*73, "Glyph outside GRED atlas");
    return static_cast<std::uint16_t>((index/40)*320 + index%40);
}
void Original::copy(Cells& cells, int source, int x, int y, int w, int h) const {
    require(source >= 0 && source%80+w <= 80 && source/80+h <= 274 &&
            x >= 0 && y >= 0 && x+w <= 80 && y+h <= 44, "Invalid original blit");
    for (int row = 0; row < h; ++row)
        std::copy_n(field_.begin()+source+row*80, w, cells.begin()+(y+row)*80+x);
}
void Original::fill(Cells& cells, int x, int y, int w, int h, unsigned index) const {
    require(x >= 0 && y >= 0 && x+w <= 80 && y+h <= 44, "Invalid original fill");
    for (int row = 0; row < h; ++row)
        std::fill_n(cells.begin()+(y+row)*80+x, w, tile(index));
}
void Original::text(Cells& cells, int x, int y, const std::vector<std::uint8_t>& value, unsigned font) const {
    for (auto c : value) {
        if (x >= 75) break;
        require(c >= 32, "Control character in original text line");
        cells[y*80+x++] = tile(font+c-32);
    }
}

Original::Cells Original::main_screen() const {
    Cells cells{};
    copy(cells, 44*80, 0, 0, 80, 44); // EXE 1000:1ac2
    // Initial menu, reconstructed from 1000:06b8 and 1000:0342.
    fill(cells, 4, 2, 71, 6, 0x44e);
    copy(cells, 0x4479, 6, 3, 69, 4);
    copy(cells, 0x4330+9, 6, 3, 20, 4);
    copy(cells, 0x4470, 15, 2, 3, 1);
    copy(cells, 0x44c0, 15, 7, 3, 1);
    copy(cells, 0x43d0, 5, 4, 1, 2);
    copy(cells, 0x43d1, 26, 4, 1, 2);
    // Initial two nonempty exercise lines (1000:0806); CP866 bytes index original glyphs.
    std::vector<std::vector<std::uint8_t>> lines;
    std::vector<std::uint8_t> line;
    for (auto c : lessons_) {
        if (c == 13) {
            if (!line.empty()) lines.push_back(line);
            line.clear();
            if (lines.size() == 2) break;
        } else if (c != 10) line.push_back(c);
    }
    require(lines.size() == 2, "Original lessons are missing");
    text(cells, 5, 12, lines[0], constant(0xa6));
    text(cells, 5, 14, lines[1], constant(0xa4));
    // Initial keyboard state from 1000:0f46: physical key positions and finger groups
    // are read from the DOS executable rather than redrawn or approximated.
    const auto expected_char = lines[0][0];
    unsigned expected = 0;
    for (unsigned i = 1; i < 59; ++i)
        if (data_[0x477+i] == expected_char) { expected = i; break; }
    const unsigned finger = data_[0xb8+expected*5+4];
    for (unsigned key = 1; key <= 65; ++key) {
        const auto* k = &data_[0xb8+key*5];
        unsigned source = constant(0x90);
        if (key == expected) source = constant(0x98);
        else if (k[4] == finger) source = constant(0x94);
        copy(cells, int(source)+k[1]*80+k[0], 5+k[0], 21+k[1], k[2], k[3]);
        if (key == expected)
            cells[(22+k[1])*80+6+k[0]] = tile(constant(0xa8)+expected_char-32);
    }
    // Caps Lock is forced on by 1000:2812; Num/Scroll Lock are off in the reference.
    cells[23*80+67] = tile(0x25d);
    cells[23*80+64] = tile(0x25e);
    cells[23*80+70] = tile(0x25e);
    return cells;
}

Frame Original::render(Screen screen) const {
    Cells cells{};
    switch (screen) {
    case Screen::intro: copy(cells, 0, 0, 0, 80, 44); break;
    case Screen::help: copy(cells, 0x47e0, 0, 0, 80, 44); break;
    case Screen::main: cells = main_screen(); break;
    }
    Frame pixels{};
    for (int y = 0; y < frame_height; ++y) for (int x = 0; x < frame_width; ++x) {
        unsigned offset = cells[(y/8)*80+x/8];
        unsigned ax = (offset%40)*8+x%8, ay = offset/40+y%8;
        pixels[y*frame_width+x] = palette_[atlas_[ay*320+ax]];
    }
    return pixels;
}
}
