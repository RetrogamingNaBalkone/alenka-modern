#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace alenka {
constexpr int frame_width = 640, frame_height = 350;
struct Rect { int x, y, w, h; };
// Display aspect is 4:3; EGA storage aspect is 64:35 (pixel aspect 35:48).
Rect fit_display(int width, int height);
using Frame = std::array<std::uint32_t, frame_width * frame_height>;
enum class Screen { intro, help, main };

class Original {
    friend class Trainer;
public:
    explicit Original(const std::filesystem::path& directory);
    Frame render(Screen screen) const;
    std::uint32_t border_color() const { return palette_[1]; }
    const std::vector<std::uint8_t>& lessons() const { return lessons_; }
private:
    std::array<std::uint32_t, 17> palette_{};
    std::vector<std::uint8_t> atlas_, data_, lessons_;
    std::vector<std::uint16_t> field_;
    using Cells = std::array<std::uint16_t, 80 * 44>;
    std::uint16_t constant(std::size_t offset) const;
    std::uint16_t tile(unsigned index) const;
    void copy(Cells& cells, int source, int x, int y, int w, int h) const;
    void fill(Cells& cells, int x, int y, int w, int h, unsigned index) const;
    void text(Cells& cells, int x, int y, const std::vector<std::uint8_t>& text, unsigned font) const;
    Cells main_screen() const;
};
}
