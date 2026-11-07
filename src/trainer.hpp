#pragma once
#include "original.hpp"
#include "profile.hpp"
#include <string>
#include <map>

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
    CourseLanguage course_language() const { return course_language_; }
    bool menu() const { return menu_; }
    bool practicing() const { return view_==View::main&&!menu_; }
    struct Sounds { bool error = false, beat = false; };
    bool sound_enabled() const { return sound_enabled_; }
    Sounds take_sounds() {
        auto result=sounds_; sounds_={};
        if(view_!=View::main) return {};
        if(menu_) result.beat=false;
        return result;
    }
    std::string diagnostic() const;
    Profile profile() const;
    void load_profile(const Profile& profile);
    void validate_profile(const Profile& profile) const;
    bool save_needed() const { return dirty_; }
    void saved() { dirty_=false; }
    bool editing_name() const { return view_==View::edit; }
    void text_input(const std::string& utf8);
    enum class ActionKind { None, List, Rename, Create, Switch, Retry, Cancel, Discard };
    struct Action { ActionKind kind=ActionKind::None; std::string value; };
    Action take_action() { auto a=action_;action_={};return a; }
    void show_profiles(const std::vector<Profile>& profiles,const std::string& warning={});
    void show_error(bool closing);
    void notify(const std::string& message,bool fatal=false);
    void edit_error(const std::string& message);
    void dismiss_error();
    std::array<unsigned,2> course_lengths() const { return {unsigned(russian_lines_.size()),unsigned(english_lines_.size())}; }
private:
    enum class View { intro, help, main, statistics, profile_menu, profiles, edit, error, message };
    const Original& original_;
    std::vector<std::uint16_t> field_;
    Original::Cells cells_{};
    std::array<bool, 104> keys_{};
    std::vector<std::vector<std::uint8_t>> lines_;
    std::vector<std::vector<std::uint8_t>> russian_lines_, english_lines_;
    CourseLanguage course_language_ = CourseLanguage::Russian;
    std::array<CharacterCount, 256> counts_{};
    Profile profile_;
    bool dirty_=false, creating_=false, closing_error_=false, fatal_message_=false;
    unsigned dialog_selection_=0;
    std::vector<Profile> profiles_;
    std::string edit_name_, dialog_message_;
    Action action_;
    struct Text { int x,y; std::string value; unsigned color; bool small; int header=-1; };
    std::vector<Text> texts_;
    struct CompactGlyph { unsigned width; std::array<unsigned,5> rows; };
    std::map<unsigned,CompactGlyph> compact_font_;
    struct HeaderBitmap { unsigned width=0; std::array<std::uint64_t,5> rows{}; };
    std::array<HeaderBitmap,6> original_headers_;
    bool help_extra_=false;
    bool startup_help_=false;
    View view_ = View::intro;
    bool running_ = true, menu_ = true, russian_ = true, caps_ = true, num_ = false, scroll_ = false;
    unsigned flags_ = 0x56, hint_ = 0, selection_ = 0, stat_selection_ = 0;
    unsigned line_ = 0, position_ = 0, ghost_ = 0, rhythm_ = 30;
    unsigned desired_modifiers_ = 0, desired_finger_ = 50;
    std::uint64_t now_ = 0, last_key_ = 0, deadline_ = 0;
    std::uint64_t repeat_deadline_ = 0;
    unsigned repeat_scan_ = 0;
    bool sound_enabled_ = true;
    Sounds sounds_{};
    std::uint64_t total_count_ = 0, total_ticks_ = 0;
    std::uint64_t current_count_ = 0, current_ticks_ = 0;

    std::uint16_t constant(unsigned offset) const { return original_.constant(offset); }
    std::uint16_t tile(unsigned index) const { return original_.tile(index); }
    void put(int x, int y, unsigned index);
    void fill(int x, int y, int w, int h, unsigned index);
    void blit(int source, int x, int y, int w, int h);
    void move(int sx, int sy, int x, int y, int w, int h);
    void draw_menu(bool active);
    void draw_main();
    void draw_preview();
    void draw_keyboard();
    void draw_key(unsigned scan);
    void draw_locks();
    void show_help();
    void show_statistics();
    void draw_statistics();
    void switch_course();
    void draw_stat_selection();
    void draw_help();
    void build_compact_font();
    unsigned compact_width(const std::string& value) const;
    void dialog_box(int x,int y,int w,int h);
    void box(int x,int y,int w,int h);
    void text(int x,int y,const std::string& value,unsigned color=14,bool small=false);
    void draw_dialog();
    void dialog_key(unsigned scan);
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
