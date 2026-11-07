#include "original.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::uint64_t fingerprint(const alenka::Frame& frame) {
    std::uint64_t value = 14695981039346656037ull;
    for (auto pixel : frame) for (unsigned shift : {0u,8u,16u,24u}) {
        value ^= (pixel >> shift) & 255;
        value *= 1099511628211ull;
    }
    return value;
}
void compare(const alenka::Frame& frame, const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(file)), {});
    // DOSBox Automation raw header is packed: w32, h32, pitch32, format8, palette_count16.
    expect(b.size() == 15+640*350*4 && b[12] == 32, "Unexpected reference framebuffer format");
    std::size_t different = 0;
    for (std::size_t i=0; i<frame.size(); ++i) {
        std::size_t p = 15+i*4;
        auto expected = 0xff000000u | (unsigned(b[p+2])<<16) | (unsigned(b[p+1])<<8) | b[p];
        different += frame[i] != expected;
    }
    std::cout << filename << ": " << different << " differing pixels\n";
    expect(different == 0, "Original framebuffer mismatch");
}
int main(int argc, char** argv) {
    try {
        expect(argc == 2 || argc == 5, "Usage: core-tests DATA [INTRO.raw HELP.raw MAIN.raw]");
        struct Case { int w,h,x,y,rw,rh; };
        for (const auto c : {
            Case{1920,1080,240,0,1440,1080}, Case{1280,720,160,0,960,720},
            Case{640,480,0,0,640,480}, Case{640,350,87,0,466,350},
            Case{800,900,0,150,800,600}, Case{600,1000,0,275,600,450},
            Case{1,1,0,0,1,0}, Case{0,0,0,0,0,0}, Case{-1,480,0,0,0,0}}) {
            auto r = alenka::fit_display(c.w,c.h);
            expect(r.x==c.x && r.y==c.y && r.w==c.rw && r.h==c.rh, "Aspect correction mismatch");
        }
        alenka::Original original(std::filesystem::u8path(argv[1]));
        expect(original.border_color() == 0xff0000aau, "Original sidebar blue changed");
        auto intro = original.render(alenka::Screen::intro);
        auto help = original.render(alenka::Screen::help);
        auto main = original.render(alenka::Screen::main);
        // Independent original DOS captures, not hashes produced by the renderer under test.
        // Provenance and raw SHA256: docs/evidence/reference-hashes.json.
        expect(fingerprint(intro) == 0x2b634ec7e360171cull, "DOS intro reference mismatch");
        expect(fingerprint(help) == 0x1ab85d9739ba92daull, "DOS help reference mismatch");
        expect(fingerprint(main) == 0x6fbff902fd845db0ull, "DOS main reference mismatch");
        expect(intro[0] == 0xff000000u, "Intro background mismatch");
        expect(main[0] == original.border_color(), "Main sidebar mismatch");
        expect(help != intro && main != help, "Original screens are identical");
        if (argc == 5) { compare(intro,argv[2]); compare(help,argv[3]); compare(main,argv[4]); }
        std::cout << "Core checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
