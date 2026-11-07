#pragma once
#include "original.hpp"
#include <string>

namespace alenka {
// Original scan-set-1 input, independent of SDL/host keyboard layout.
class Trainer {
    friend struct TrainerFixture;
public:
    explicit Trainer(const Original& original, Screen start = Screen::intro);
    void key(unsigned scan, bool pressed, std::uint64_t milliseconds);
    void advance(std::uint64_t milliseconds);
    void release_keys();
    Frame frame() const;
    bool running() const { return running_; }
    unsigned line() const { return line_; }
    unsigned position() const { return position_; }
    unsigned flags() const { return flags_; }
    bool menu() const { return menu_; }
    std::string diagnostic() const;
private:
    enum class View { intro, help, main, statistics, unavailable };
    const Original& original_;
    std::vector<std::uint16_t> field_;
    Original::Cells cells_{};
    std::array<bool, 104> keys_{};
    std::vector<std::vector<std::uint8_t>> lines_;
    struct Count { std::uint32_t ticks = 0; std::uint16_t correct = 0, errors = 0; };
    std::array<Count, 256> counts_{};
    View view_ = View::intro, help_return_ = View::main;
    bool running_ = true, menu_ = true, russian_ = true, caps_ = true, num_ = false, scroll_ = false;
    unsigned flags_ = 0x56, hint_ = 0, selection_ = 0, stat_selection_ = 0;
    unsigned line_ = 0, position_ = 0, ghost_ = 0, rhythm_ = 30;
    unsigned desired_modifiers_ = 0, desired_finger_ = 50;
    std::uint64_t now_ = 0, last_key_ = 0, deadline_ = 0;
    std::uint64_t repeat_deadline_ = 0;
    unsigned repeat_scan_ = 0;
    std::uint32_t total_count_ = 0, total_ticks_ = 0;
    unsigned current_count_ = 0, current_ticks_ = 0;

    std::uint16_t constant(unsigned offset) const { return original_.constant(offset); }
    std::uint16_t tile(unsigned index) const { return original_.tile(index); }
    void put(int x, int y, unsigned index);
    void fill(int x, int y, int w, int h, unsigned index);
    void blit(int source, int x, int y, int w, int h);
    void move(int sx, int sy, int x, int y, int w, int h);
    void field_blit(int source, int destination, int w, int h);
    void number(int destination, unsigned width, std::uint32_t value, unsigned font, unsigned blank);
    void menu_number(unsigned selection, std::uint32_t value, unsigned width);
    void menu_toggle(unsigned selection, bool enabled);
    void draw_menu(bool active);
    void draw_main();
    void draw_preview();
    void draw_keyboard();
    void draw_key(unsigned scan);
    void draw_locks();
    void show_help();
    void show_statistics();
    void draw_statistics();
    void draw_stat_selection();
    void update_setting(int direction, bool space);
    unsigned modifiers() const;
    unsigned layout(unsigned modifiers) const;
    unsigned character(unsigned scan) const;
    unsigned find_key(unsigned character, unsigned modifiers) const;
    unsigned expected() const;
    unsigned expected_key() const;
    void start_line();
    void consume(unsigned scan, bool timeout);
    void finish_line(bool completed);
    void choose_line(int direction);
};
}
