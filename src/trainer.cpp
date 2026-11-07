#include "trainer.hpp"
#include "english_course.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <limits>

namespace alenka {
namespace {
enum class Field { Profile, Language, Sound, Typos, Difficulty, Hint, Statistics, Line, Rhythm };
struct MenuItem { Field field; unsigned width; const char* label; };
constexpr MenuItem menu_items[]={
    {Field::Profile,11,u8"Профиль"},{Field::Language,5,u8"Язык"},{Field::Sound,5,u8"Звук"},
    {Field::Typos,7,u8"Опечатки"},{Field::Difficulty,7,u8"Сложность"},{Field::Hint,7,u8"Подсказка"},
    {Field::Statistics,7,u8"Статистика"},{Field::Line,6,u8"Строка"},{Field::Rhythm,6,u8"Ритм"}
};
constexpr unsigned menu_size=sizeof(menu_items)/sizeof(menu_items[0]);
}
Trainer::Trainer(const Original& original, Screen start) : original_(original), field_(original.field_) {
    build_compact_font();
    std::vector<std::uint8_t> line;
    // 1000:27a1 explicitly skips the file's initial CRLF. Interior empty
    // lines remain lessons and retain their original line numbers.
    for (std::size_t i=2;i<original.lessons_.size();++i) {
        auto c=original.lessons_[i];
        if (c == 13) { lines_.push_back(line); line.clear(); }
        else if (c != 10 && c != 26) line.push_back(c);
    }
    if (!line.empty()) lines_.push_back(line);
    if (lines_.empty()) throw std::runtime_error("Empty original lesson file");
    russian_lines_=lines_;
    for(const auto* text:english_lines) {
        std::string value(text);
        english_lines_.emplace_back(value.begin(),value.end());
    }
    if (start == Screen::main) { view_ = View::main; draw_main(); }
    else if (start == Screen::help) { view_ = View::help;startup_help_=true;draw_help(); }
    else blit(0,0,0,80,44);
}
void Trainer::put(int x, int y, unsigned index) {
    if (x >= 0 && x < 80 && y >= 0 && y < 44) cells_[y*80+x] = tile(index);
}
void Trainer::fill(int x,int y,int w,int h,unsigned index) {
    for (int row=0;row<h;++row) for (int col=0;col<w;++col) put(x+col,y+row,index);
}
void Trainer::blit(int source,int x,int y,int w,int h) {
    for (int row=0;row<h;++row) for (int col=0;col<w;++col) {
        int src = source+row*80+col;
        if (src >= 0 && std::size_t(src)<field_.size() && x+col>=0 && x+col<80 && y+row>=0 && y+row<44)
            cells_[(y+row)*80+x+col] = field_[src];
    }
}
void Trainer::move(int sx,int sy,int x,int y,int w,int h) {
    auto previous=cells_;
    for (int row=0;row<h;++row) for(int col=0;col<w;++col)
        cells_[(y+row)*80+x+col]=previous[(sy+row)*80+sx+col];
}
void Trainer::draw_menu(bool active) {
    texts_.clear();
    fill(4,2,71,6,0x44e);
    auto name=profile_.name;
    if(name_glyphs(name).size()>9) {
        // Cut on UTF-8 boundaries, never in the middle of a Russian letter.
        std::size_t end=0;
        for(unsigned n=0;n<8;++n) end+=static_cast<unsigned char>(name[end])>=128?2:1;
        name=name.substr(0,end)+">";
    }
    const std::string values[]={name,"",
        "",(flags_&0x10)?"OFF":"ON",(flags_&0x40)?"OFF":"ON",
        std::to_string(hint_),"",std::to_string(line_),std::to_string(rhythm_)};
    unsigned x=6;
    for(unsigned i=0;i<menu_size;++i) {
        unsigned w=menu_items[i].width;std::string label=menu_items[i].label;box(int(x),3,int(w),4);
        if(i>=3) {
            const auto& header=original_headers_[i-3];
            texts_.push_back({int(x*8+(w*8-header.width)/2),30,"",active?0u:8u,false,int(i-3)});
        } else text(int(x*8+(w*8-compact_width(label))/2),30,label,active?0:8,true);
        auto value_count=values[i].size();
        for(auto c:values[i]) if((static_cast<unsigned char>(c)&0xc0)==0x80) --value_count;
        text(int(x*8+(w*8-value_count*8)/2),40,values[i],active?15:8);
        if(active&&selection_==i) {
            blit(0x4470,int(x+w/2)-1,2,3,1);blit(0x44c0,int(x+w/2)-1,7,3,1);
            blit(0x43d0,int(x)-1,4,1,2);blit(0x43d1,int(x+w),4,1,2);
        }
        x+=w+1;
    }
}
void Trainer::draw_preview() {
    fill(5,11,70,5,constant(0xa4));
    for(unsigned i=0;i<std::min<std::size_t>(70,lines_[line_].size());++i)
        put(5+int(i),12,constant(0xa6)+lines_[line_][i]-32);
    if(line_+1<lines_.size()) for(unsigned i=0;i<std::min<std::size_t>(70,lines_[line_+1].size());++i)
        put(5+int(i),14,constant(0xa4)+lines_[line_+1][i]-32);
}
void Trainer::draw_main() {
    blit(44*80,0,0,80,44);
    draw_menu(menu_);
    draw_preview();
    draw_keyboard();
    draw_locks();
}
unsigned Trainer::modifiers() const {
    return (caps_?0x40:0)|(keys_[42]?2:0)|(keys_[54]?1:0);
}
unsigned Trainer::layout(unsigned mods) const {
    return (russian_?0x401:0x315)+((mods&0x40)?118:0)+((mods&3)?59:0);
}
unsigned Trainer::character(unsigned scan) const {
    return original_.data_[layout(modifiers())+scan];
}
unsigned Trainer::find_key(unsigned c,unsigned mods) const {
    for(unsigned i=1;i<59;++i) if(original_.data_[layout(mods)+i]==c) return i;
    return 0;
}
unsigned Trainer::expected() const {
    return position_<lines_[line_].size()?lines_[line_][position_]:13;
}
unsigned Trainer::expected_key() const { return find_key(expected(),modifiers()); }
void Trainer::draw_keyboard() {
    unsigned desired=expected(), mods=modifiers();
    unsigned scan=find_key(desired,mods);
    desired_modifiers_=mods;
    desired_finger_=scan?original_.data_[0xbc+scan*5]:50;
    if(!scan) {
        unsigned other=mods^((mods&3)?(mods&3):3);
        scan=find_key(desired,other);
        if(scan) {
            if(mods&3) desired_modifiers_&=~3u;
            else desired_modifiers_|=original_.data_[0xbc+scan*5]<=4?1:2;
        }
    }
    for(unsigned key=1;key<=65;++key) draw_key(key);
}
void Trainer::draw_key(unsigned scan) {
    const auto* k=&original_.data_[0xb8+scan*5];
    const unsigned c=character(scan);
    const bool special_shift=(scan==54&&((desired_modifiers_^modifiers())&1)) ||
                              (scan==42&&((desired_modifiers_^modifiers())&2));
    const bool hint=(flags_&2) && k[4]==desired_finger_ && expected_key();
    unsigned source=constant(keys_[scan]?0x92:0x90);
    if(special_shift||hint) source=constant(keys_[scan]?0x96:0x94);
    if(!keys_[scan]&&hint&&(flags_&4)&&expected_key()==scan) source=constant(0x98);
    blit(int(source)+k[1]*80+k[0],5+k[0],21+k[1],k[2],k[3]);
    if(scan>57 || c==126 || special_shift) return;
    unsigned font=0;
    if(keys_[scan]) font=constant(hint?0xae:0xaa);
    else if(hint&&(flags_&4)&&expected_key()==scan) font=constant(0xa8);
    else if(hint) {
        if(!(flags_&1)) { put(6+k[0],22+k[1],constant(0xae)); return; }
        font=constant(0xae);
    } else if(flags_&1) font=constant(0xac);
    if(font || (flags_&1)) put(6+k[0],22+k[1],font+c-32);
    else put(6+k[0],22+k[1],0);
}
void Trainer::draw_locks() {
    put(67,23,caps_?0x25d:0x25e);
    put(64,23,num_?0x25d:0x25e);
    put(70,23,scroll_?0x25d:0x25e);
}
void Trainer::show_help() {
    if(practicing()) { menu_=true;position_=0;dirty_=true; }
    view_=View::help;
    menu_=true;
    position_=0;
    help_extra_=false;
    startup_help_=false;
    draw_help();
}
void Trainer::show_statistics() {
    menu_=true;
    position_=0;
    view_=View::statistics;
    draw_statistics();
}
void Trainer::draw_stat_selection() {
    int x=stat_selection_?41:31,w=8;
    blit(0x4470,x+w/2-1,1,3,1);blit(0x44c0,x+w/2-1,6,3,1);
    blit(0x43d0,x-1,3,1,2);blit(0x43d1,x+w,3,1,2);
}
void Trainer::switch_course() {
    auto snapshot=profile();profile_.courses=snapshot.courses;
    course_language_=course_language_==CourseLanguage::Russian?CourseLanguage::English:CourseLanguage::Russian;
    russian_=course_language_==CourseLanguage::Russian;
    lines_=russian_?russian_lines_:english_lines_;
    const auto& progress=profile_.courses[course_language_==CourseLanguage::English?1:0];
    line_=progress.line;counts_=progress.counts;total_count_=progress.total_count;total_ticks_=progress.total_ticks;
    position_=ghost_=0;current_count_=current_ticks_=0;
    last_key_=deadline_=repeat_deadline_=0;
    repeat_scan_=0; sounds_={}; menu_=true;
    // Keep physical held-key state: repeated key-down events must not toggle
    // again, and the eventual key-up must not become exercise input.
    dirty_=true;draw_main();
}
void Trainer::draw_statistics() {
    texts_.clear();
    blit(0x3570,0,0,80,44);
    fill(0,1,80,6,0x44e);
    blit(0x3570+2*80+5,31,2,8,4);blit(0x3570+2*80+14,41,2,8,4);
    fill(0,18,37,21,0x44e); fill(42,18,37,21,0x44e);
    // Character columns are the original finger-group ordering in DS:02bb.
    long double max_average=0;
    for(unsigned i=0;i<44;++i) {
        const auto& count=counts_[character(original_.data_[0x2bb+i*2])];
        long double average=count.correct?count.ticks/count.correct:0;
        max_average=std::max(max_average,average+count.errors);
    }
    auto bar=[&](unsigned x,unsigned correct,unsigned errors) {
        unsigned y=37;
        for(unsigned i=0;i<correct/4&&y>=18;++i) put(int(x),int(y--),0x52b);
        unsigned remainder=correct%4;
        if(errors+remainder>3) {
            put(int(x),int(y--),0x52f+remainder);
            errors-=4-remainder;
            for(unsigned i=0;i<errors/4&&y>=18;++i) put(int(x),int(y--),0x52f);
            if(errors%4&&y>=18) put(int(x),int(y),0x52b+errors%4);
        } else if(errors) {
            if(!remainder) put(int(x),int(y),0x52b+errors);
            else if(errors+remainder==3) put(int(x),int(y),0x532+remainder);
            else if(errors+remainder==2) put(int(x),int(y),0x534+remainder);
        } else if(remainder) put(int(x),int(y),0x527+remainder);
    };
    for(unsigned i=0;i<44;++i) {
        unsigned scan=original_.data_[0x2bb+i*2], x=original_.data_[0x2bc+i*2];
        unsigned c=character(scan);
        auto count=counts_[c];
        put(int(x),39,count.correct||count.errors?0x44e + c-32:0x44e);
        if(max_average&&(count.correct||count.errors))
            bar(x,count.correct?unsigned((count.ticks/count.correct)*80.0L/max_average):0,
                  unsigned(count.errors*80.0L/max_average));
    }
    draw_stat_selection();
}
void Trainer::update_setting(int direction,bool space) {
    switch(menu_items[selection_].field) {
    case Field::Profile: view_=View::profile_menu;dialog_selection_=0;draw_dialog();return;
    case Field::Language: switch_course();return;
    case Field::Sound: sound_enabled_=!sound_enabled_;sounds_={};break;
    case Field::Typos: flags_^=0x10; break;
    case Field::Difficulty: flags_^=0x40; break;
    case Field::Hint:
        if(space) return;
        hint_=(hint_+(direction>0?1:2))%3;
        flags_=(flags_&0xf0)|constant(0xc18+hint_*2);
        draw_keyboard(); break;
    case Field::Statistics: stat_selection_=0;show_statistics(); return;
    case Field::Line:
        if(space) return;
        choose_line(direction>0?-1:1); break;
    case Field::Rhythm:
        if(space) return;
        if(direction>0) rhythm_=std::min(400u,rhythm_+10);
        else rhythm_=rhythm_>10?rhythm_-10:10;
        break;
    default: return;
    }
    dirty_=true;draw_menu(true);
}
void Trainer::choose_line(int direction) {
    if(direction<0&&!line_) return;
    if(direction<0&&line_) --line_;
    else if(direction>0&&line_+1<lines_.size()) ++line_;
    position_=0;
    dirty_=true;
    draw_preview(); draw_keyboard();
}
void Trainer::start_line() {
    menu_=false;
    position_=ghost_=0; current_count_=current_ticks_=0;
    draw_menu(false);
    put(5,11,constant(0xb2));
    put(5,12,constant(0xa8)+expected()-32);
    put(5,13,constant(0xb4));
    draw_preview();
    deadline_=now_*1193182/16384000+4380/rhythm_;
    if(sound_enabled_) sounds_.beat=true;
}
void Trainer::consume(unsigned scan,bool timeout) {
    if(expected()==13) return;
    if(timeout&&sound_enabled_) sounds_.error=true;
    unsigned c=expected();
    bool correct=!timeout&&scan==expected_key();
    auto tick=now_*1193182/16384000;
    unsigned ticks=std::uint16_t(tick-last_key_);
    // Keep aggregates and character counts consistent even at the storage limit.
    if(total_count_!=std::numeric_limits<std::uint64_t>::max()&&
       total_ticks_<=std::numeric_limits<std::uint64_t>::max()-ticks) {
        ++total_count_;total_ticks_+=ticks;++current_count_;current_ticks_+=ticks;
        if(correct) { counts_[c].ticks+=ticks;++counts_[c].correct; }
        else ++counts_[c].errors;
    }
    if(!(flags_&0x40)) {
        put(5+int(position_),11,constant(0xb6)); put(5+int(position_),13,constant(0xb6));
    }
    unsigned shown=correct?constant(0xa4)+c:constant(0xb0)+character(scan);
    put(5+int(position_),12,shown-32);
    ++position_;
    if(correct||timeout) last_key_=tick;
    if(expected()==13) {
        put(5+int(position_),12,0x618);
        finish_line(true);
    } else {
        put(5+int(position_),12,constant(0xa8)+expected()-32);
        if(!(flags_&0x40)) {
            put(5+int(position_),11,constant(0xb2)); put(5+int(position_),13,constant(0xb4));
        }
        draw_keyboard();
    }
    if(!(flags_&0x40)) deadline_=tick+4380/rhythm_;
}
void Trainer::finish_line(bool completed) {
    dirty_=true;
    move(5,12,5,11,70,4);
    if(completed&&line_+1<lines_.size()) ++line_;
    // The original leaves the scrolled current/next lines visible until input
    // resumes; it only draws the following line at row 15 here.
    fill(5,15,70,1,constant(0xa4));
    unsigned following=line_+(completed?1:2);
    if(following<lines_.size()) for(unsigned i=0;i<std::min<std::size_t>(70,lines_[following].size());++i)
        put(5+int(i),15,constant(0xa4)+lines_[following][i]-32);
    position_=0; menu_=true; draw_menu(true); draw_keyboard();
}
void Trainer::advance(std::uint64_t milliseconds) {
    now_=milliseconds;
    while(repeat_scan_ && view_==View::main && menu_ && keys_[repeat_scan_] &&
          now_*1193182/16384000>=repeat_deadline_) {
        choose_line((repeat_scan_==72||repeat_scan_==95)?-1:1);
        draw_menu(true);
        ++repeat_deadline_;
    }
    while(view_==View::main&&!menu_&&deadline_&&now_*1193182/16384000>=deadline_) {
        if(sound_enabled_) sounds_.beat=true;
        auto current=now_;
        auto due=deadline_;
        now_=(due*16384000+1193181)/1193182;
        if(!(flags_&0x40)) consume(expected_key(),true);
        else {
            put(5+int(ghost_),11,constant(0xb6)); put(5+int(ghost_),13,constant(0xb6));
            if(ghost_<lines_[line_].size()) ++ghost_;
            put(5+int(ghost_),11,constant(0xb2)); put(5+int(ghost_),13,constant(0xb4));
        }
        now_=current;
        deadline_=due+4380/rhythm_;
    }
}
void Trainer::release_keys() {
    keys_.fill(false);
    repeat_scan_=0;
    if(view_==View::main) { draw_keyboard(); draw_locks(); }
    else if(view_==View::statistics) draw_statistics();
}
void Trainer::key(unsigned scan,bool pressed,std::uint64_t milliseconds) {
    advance(milliseconds);
    if(scan==0||scan>=keys_.size()||keys_[scan]==pressed) return;
    keys_[scan]=pressed;
    if(!pressed && scan==repeat_scan_) repeat_scan_=0;
    if(pressed && repeat_scan_ && scan!=repeat_scan_) repeat_scan_=0;
    if(view_==View::intro) {
        if(!pressed&&(scan==28||scan==57)) { view_=View::help;startup_help_=true;draw_help(); }
        return;
    }
    if(view_==View::help) {
        if(pressed&&(scan==1||(startup_help_&&(scan==28||scan==100)))) {
            startup_help_=false;view_=View::main;draw_main();
        }
        else if(pressed&&(scan==75||scan==92||scan==77||scan==99)) { help_extra_=!help_extra_;draw_help(); }
        return;
    }
    if(view_!=View::main&&view_!=View::statistics) { if(pressed) dialog_key(scan);return; }
    if(pressed) {
        if(scan==58) caps_=!caps_;
        if(scan==69) num_=!num_;
        if(scan==70) scroll_=!scroll_;
        if((scan==42||scan==54)&&keys_[42]&&keys_[54]) russian_=!russian_;
    }
    if(view_==View::statistics) {
        if(!pressed) return;
        if(scan==1) { view_=View::main;draw_main();return; }
        if(scan==59) { show_help(); return; }
        if(scan==28||scan==100) { view_=View::main; draw_main(); return; }
        if(scan==75||scan==92||scan==77||scan==99) {
            stat_selection_=1-stat_selection_;
            draw_statistics(); return;
        }
        if(scan==42||scan==54||scan==58) { draw_statistics(); return; }
        if(scan==57||scan==72||scan==80||scan==95||scan==96) {
            if(stat_selection_==0) { view_=View::main; draw_main(); }
            else { counts_={};total_count_=total_ticks_=0;dirty_=true;draw_statistics(); }
        }
        return;
    }
    draw_keyboard(); draw_key(scan); draw_locks();
    if(scan==28) blit(int(constant(pressed?0x92:0x90))+7*80+40,45,28,1,3);
    if(!pressed) return;
    if(scan==59) { show_help(); return; }
    if(scan==1) {
        if(!menu_) finish_line(false);
        else { stat_selection_=0;show_statistics(); }
        return;
    }
    if(scan==28||scan==100) {
        if(menu_) {
            start_line(); last_key_=now_*1193182/16384000;
            if(expected()==13) finish_line(true);
        }
        else finish_line(false);
        return;
    }
    if(scan==42||scan==54||scan==58||scan==69||scan==70) return;
    if(menu_) {
        if(scan==75||scan==92||scan==77||scan==99) {
            selection_=(selection_+((scan==75||scan==92)?menu_size-1:1))%menu_size;
            draw_menu(true); return;
        }
        if(scan==72||scan==95||scan==80||scan==96) {
            update_setting((scan==72||scan==95)?1:-1,false);
            if(menu_items[selection_].field==Field::Line) { repeat_scan_=scan; repeat_deadline_=now_*1193182/16384000+24; }
            return;
        }
        if(scan==57) { update_setting(1,true); return; }
        if(scan==expected_key()) { start_line(); consume(scan,false); }
    } else {
        if(scan!=expected_key()&&sound_enabled_) sounds_.error=true;
        if(scan==expected_key() || !(flags_&0x10)) consume(scan,false);
    }
}
Frame Trainer::frame() const {
    Frame pixels{};
    for(int y=0;y<frame_height;++y) for(int x=0;x<frame_width;++x) {
        unsigned offset=cells_[(y/8)*80+x/8];
        unsigned ax=(offset%40)*8+x%8, ay=offset/40+y%8;
        pixels[y*frame_width+x]=original_.palette_[original_.atlas_[ay*320+ax]];
    }
    // Keep the original language and speaker artwork in the compact main menu.
    if(view_!=View::intro&&view_!=View::help&&view_!=View::statistics) {
        auto artwork=[&](int sx,int sy,int w,int h,int dx,int dy,bool speaker) {
            for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                auto off=field_[0x3570+(sy+y)/8*80+(sx+x)/8];
                unsigned color=original_.atlas_[(off/40+(sy+y)%8)*320+(off%40)*8+(sx+x)%8];
                if(speaker?color!=7:(color==14||color==4))
                    pixels[(dy+y)*640+dx+x]=original_.palette_[speaker&&color==9?1:color];
            }
        };
        if(course_language_==CourseLanguage::Russian) artwork(201,25,32,15,150,38,false);
        else for(unsigned color:{4u,14u}) {
            int shadow=color==4?1:0;
            for(int y=0;y<12;++y) for(int x=0;x<23;++x) if(eng_button[y][x]=='#')
                pixels[(38+y+shadow)*640+152+x+shadow]=original_.palette_[color];
        }
        artwork(270,23,33,18,196,35,true);
        if(!sound_enabled_) for(int x=0;x<33;++x) {
            int y=51-x/2;
            pixels[y*640+196+x]=original_.palette_[4];
        }
    }
    for(const auto& t:texts_) {
        if(t.header>=0) {
            const auto& header=original_headers_[unsigned(t.header)];
            for(unsigned y=0;y<5;++y) for(unsigned x=0;x<header.width;++x)
                if(header.rows[y]&(std::uint64_t(1)<<x)) pixels[(t.y+y)*640+t.x+x]=original_.palette_[t.color];
            continue;
        }
        int cursor=t.x;
        for(std::size_t i=0;i<t.value.size();) {
            unsigned c=static_cast<unsigned char>(t.value[i++]);
            if(c>=0xc0&&i<t.value.size()) c=((c&31)<<6)|(static_cast<unsigned char>(t.value[i++])&63);
            if(c>=0x410&&c<=0x43f) c=c-0x410+0x80;
            else if(c>=0x440&&c<=0x44f) c=c-0x440+0xe0;
            else if(c==0x401) c=0xf0;
            else if(c==0x451) c=0xf1;
            else if(c<32||c>126) c='?';
            if(t.small) {
                unsigned upper=c;
                if(c>=0xa0&&c<=0xaf) upper=c-32;
                else if(c>=0xe0&&c<=0xef) upper=c-80;
                else if(c==0xf1) upper=0xf0;
                auto found=compact_font_.find(upper);
                if(found!=compact_font_.end()) {
                    const auto& glyph=found->second;
                    for(int y=0;y<5;++y) for(unsigned x=0;x<glyph.width;++x)
                        if((glyph.rows[y]&(1u<<x))&&cursor+int(x)>=0&&cursor+int(x)<640&&t.y+y>=0&&t.y+y<350)
                            pixels[(t.y+y)*640+cursor+x]=original_.palette_[t.color];
                    cursor+=int(glyph.width)+1;continue;
                }
            }
            // Use only the ink of the original regular font. Its white relief
            // is decoration, not part of the letter mask; never merge the two.
            bool yo=c==0xf0||c==0xf1;
            unsigned character=yo?(c==0xf0?0x85:0xa5):c;
            auto offset=tile(character-32);
            for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
                bool ink;
                if(yo&&y<2) ink=y==0&&(x==2||x==5);
                else {
                    int sy=y-(yo?2:0);
                    auto source=(offset/40+sy)*320+(offset%40)*8+x;
                    ink=original_.atlas_[source]==0;
                }
                if(ink&&cursor+x>=0&&cursor+x<640&&t.y+y>=0&&t.y+y<350)
                    pixels[(t.y+y)*640+cursor+x]=original_.palette_[t.color];
            }
            cursor+=8;
        }
    }
    return pixels;
}
void Trainer::text(int x,int y,const std::string& value,unsigned color,bool small) {
    texts_.push_back({x,y,value,color,small});
}
void Trainer::box(int x,int y,int w,int h) {
    fill(x,y,w,h,0x660);
    for(int i=1;i<w-1;++i) { put(x+i,y,0x610);put(x+i,y+h-1,0x60c); }
    for(int i=1;i<h-1;++i) { put(x,y+i,0x3dd);put(x+w-1,y+i,0x60e); }
    put(x,y,0x60f);put(x+w-1,y,0x611);put(x,y+h-1,0x60b);put(x+w-1,y+h-1,0x60d);
}
Profile Trainer::profile() const {
    auto result=profile_;
    result.language=course_language_;result.settings={flags_,hint_,rhythm_,sound_enabled_};
    auto& course=result.courses[course_language_==CourseLanguage::English?1:0];
    course.line=line_;course.counts=counts_;course.total_count=total_count_;course.total_ticks=total_ticks_;
    return result;
}
void Trainer::validate_profile(const Profile& p) const {
    if(p.courses[0].line>=russian_lines_.size()||p.courses[1].line>=english_lines_.size())
        throw std::runtime_error("Profile line is outside the course");
}
void Trainer::load_profile(const Profile& p) {
    validate_profile(p);profile_=p;course_language_=p.language;
    const auto& progress=p.courses[course_language_==CourseLanguage::English?1:0];
    line_=progress.line;counts_=progress.counts;total_count_=progress.total_count;total_ticks_=progress.total_ticks;
    flags_=p.settings.flags;hint_=p.settings.hint;rhythm_=p.settings.rhythm;sound_enabled_=p.settings.sound;
    russian_=course_language_==CourseLanguage::Russian;lines_=russian_?russian_lines_:english_lines_;
    menu_=true;position_=ghost_=0;current_count_=current_ticks_=0;
    last_key_=deadline_=repeat_deadline_=0;repeat_scan_=0;sounds_={};dirty_=false;
    if(view_!=View::intro&&view_!=View::help) { view_=View::main;draw_main(); }
}
void Trainer::build_compact_font() {
    auto extract=[&](unsigned column,unsigned width,const std::string& word) {
        auto letters=name_glyphs(word);
        auto ink=[&](unsigned x,unsigned y) {
            auto off=field_[220*80+column+x/8];
            return original_.atlas_[(off/40+y)*320+(off%40)*8+x%8]==0;
        };
        auto occupied=[&](unsigned x) { for(unsigned y=0;y<5;++y) if(ink(x,y)) return true;return false; };
        unsigned x=4;
        for(unsigned char letter:letters) {
            while(x<width-2&&!occupied(x)) ++x;
            unsigned start=x;while(x<width-2&&occupied(x)) ++x;
            if(x==start||x-start>8) throw std::runtime_error("Cannot extract original compact font");
            CompactGlyph glyph{x-start,{}};
            for(unsigned y=0;y<5;++y) for(unsigned col=0;col<glyph.width;++col)
                if(ink(start+col,y)) glyph.rows[y]|=1u<<col;
            compact_font_.emplace(letter,glyph);
        }
    };
    extract(30,56,u8"ОПЕЧАТКИ");extract(38,56,u8"СЛОЖНОСТЬ");
    extract(70,56,u8"РИТМ");extract(9,160,u8"ИМЯ");
    auto field_ink=[&](unsigned column,unsigned x,unsigned y) {
        auto off=field_[220*80+column+x/8];
        return original_.atlas_[(off/40+y)*320+(off%40)*8+x%8]==0;
    };
    CompactGlyph z{4,{}};
    for(unsigned y=0;y<5;++y) for(unsigned x=0;x<4;++x) if(field_ink(46,35+x,y)) z.rows[y]|=1u<<x;
    compact_font_[0x87]=z;
    for(unsigned i=0;i<6;++i) {
        unsigned column=original_.data_[0xc22+(i+1)*2],left=54,right=4;
        for(unsigned y=0;y<5;++y) for(unsigned x=4;x<54;++x) if(field_ink(column,x,y)) {
            left=std::min(left,x);right=std::max(right,x);
        }
        auto& header=original_headers_[i];header.width=right-left+1;
        for(unsigned y=0;y<5;++y) for(unsigned x=0;x<header.width;++x)
            if(field_ink(column,left+x,y)) header.rows[y]|=std::uint64_t(1)<<x;
    }
    // Four letters absent from the precomposed labels, drawn on their native
    // 4/5-by-5 grid. Every other heading glyph is copied from the original.
    compact_font_[0x94]={5,{14,21,21,14,4}}; // Ф
    compact_font_[0x82]={4,{7,9,7,9,7}};    // В
    compact_font_[0x93]={4,{9,9,14,8,7}};   // У
    compact_font_[0x9b]={5,{17,17,23,21,23}}; // Ы
}
unsigned Trainer::compact_width(const std::string& value) const {
    unsigned width=0;
    for(unsigned char c:name_glyphs(value)) {
        if(c>=0xa0&&c<=0xaf) c-=32;
        else if(c>=0xe0&&c<=0xef) c-=80;
        auto found=compact_font_.find(c);
        width+=(found==compact_font_.end()?8:found->second.width)+1;
    }
    return width?width-1:0;
}
void Trainer::dialog_box(int x,int y,int w,int h) {
    fill(x,y,w,h,0);
    for(int i=1;i<w-1;++i) { blit(0x47e0+1,x+i,y,1,1);blit(0x47e0+43*80+1,x+i,y+h-1,1,1); }
    for(int i=1;i<h-1;++i) { blit(0x47e0+80,x,y+i,1,1);blit(0x47e0+80+79,x+w-1,y+i,1,1); }
    blit(0x47e0,x,y,1,1);blit(0x47e0+79,x+w-1,y,1,1);
    blit(0x47e0+43*80,x,y+h-1,1,1);blit(0x47e0+43*80+79,x+w-1,y+h-1,1,1);
}
void Trainer::draw_help() {
    texts_.clear();blit(0x47e0,0,0,80,44);
    auto blank=original_.field_[0x47e0+38*80+2];
    auto clear=[&](int x,int y,int w,int h) {
        for(int row=y;row<y+h;++row) for(int col=x;col<x+w;++col) cells_[row*80+col]=blank;
    };
    if(!help_extra_) {
        // Preserve the original hand-position and key illustrations in full.
        clear(8,34,70,3);
        text(72,272,u8"Esc - вернуться в основное меню.",0);
        text(72,284,u8"Alt+F4 - сохранить профиль и выйти.",0);
    } else {
        clear(1,1,78,37);
        text(24,16,u8"ПРОФИЛИ И СОХРАНЕНИЯ",0);
        const std::string lines[]={u8"Вверх/вниз в поле Профиль - переименовать или сменить.",
            u8"В списке можно выбрать профиль или создать новый.",u8"Enter - подтвердить. Esc - отменить.",
            u8"Имя: до 32 букв и цифр, пробел, дефис и подчёркивание.",u8"Имя вводится в системной раскладке Windows.",
            u8"Язык и звук переключаются в верхнем меню.",u8"Строка и статистика РУС и ENG сохраняются отдельно.",
            u8"Учебные настройки и звук общие для обоих курсов.",u8"Сохранение - после строки и изменения настройки.",
            u8"При выходе посреди строки статистика сохраняется.",u8"При открытии эта строка начинается сначала.",
            u8"Esc при наборе - меню. Esc в меню - статистика.",u8"Esc в справке и статистике - основное меню.",
            u8"Alt+F4 - сохранить и выйти. Alt+Enter - полный экран."};
        int y=48;for(const auto& line:lines) { text(24,y,line,0);y+=16; }
    }
    clear(1,41,78,2);
    text(176,328,startup_help_?u8"Enter / Esc - меню. Влево/вправо - страницы.":
        (help_extra_?u8"Влево - исходная справка. Esc - меню.":u8"Вправо - профили и сохранения. Esc - меню."),0);
}
void Trainer::show_profiles(const std::vector<Profile>& profiles,const std::string& warning) {
    profiles_=profiles;view_=View::profiles;dialog_selection_=0;
    for(unsigned i=0;i<profiles_.size();++i) if(profiles_[i].id==profile_.id) dialog_selection_=i+1;
    dialog_message_=warning;draw_dialog();
}
void Trainer::show_error(bool closing) {
    menu_=true;position_=0;repeat_scan_=0;sounds_={};closing_error_=closing;
    view_=View::error;dialog_selection_=0;draw_dialog();
}
void Trainer::notify(const std::string& message,bool fatal) {
    menu_=true;view_=View::message;dialog_message_=message;fatal_message_=fatal;draw_dialog();
}
void Trainer::edit_error(const std::string& message) { dialog_message_=message;view_=View::edit;draw_dialog(); }
void Trainer::dismiss_error() { if(view_==View::error) { view_=View::main;draw_main(); } }
void Trainer::draw_dialog() {
    draw_main();
    if(view_==View::profile_menu) {
        dialog_box(6,9,24,11);text(64,88,u8"Профиль",0);
        const std::string items[]={u8"Переименовать",u8"Сменить"};
        for(unsigned i=0;i<2;++i) {
            int y=104+int(i)*16;
            if(i==dialog_selection_) fill(7,y/8,22,1,0x44e);
            text(72,y,items[i],i==dialog_selection_?15:0);
        }
        text(64,140,"Enter / Esc",0);return;
    }
    if(view_==View::edit) {
        bool error=!dialog_message_.empty();
        dialog_box(18,10,44,error?15:13);
        text(160,96,creating_?u8"Новый профиль":u8"Переименовать профиль",0);
        dialog_box(19,14,42,3);text(160,120,edit_name_+"_",0);
        text(160,144,u8"Имя: от 1 до 32 символов",0);
        if(error) text(160,160,dialog_message_,4);
        text(160,error?184:168,u8"Enter - принять   Esc - отмена",0);return;
    }
    bool list=view_==View::profiles;
    dialog_box(13,list?10:12,54,list?25:(view_==View::message?11:16));
    text(120,list?96:112,list?u8"Сменить профиль":u8"Профиль",0);
    if(view_==View::message) {
        text(120,136,dialog_message_,0);text(120,160,u8"Enter / Esc - закрыть",0);return;
    }
    std::vector<std::string> items;
    if(view_==View::profiles) {
        if(!dialog_message_.empty()) text(120,112,dialog_message_,4);
        items.push_back(u8"Новый профиль...");
        for(const auto& p:profiles_) items.push_back((p.id==profile_.id?"> ":"  ")+p.name);
    } else {
        text(120,128,u8"Не удалось сохранить данные",4);
        items={u8"Повторить",u8"Отменить"};if(closing_error_) items.push_back(u8"Выйти без сохранения");
    }
    unsigned first=dialog_selection_>=8?dialog_selection_-7:0;
    for(unsigned i=first;i<items.size()&&i<first+8;++i) {
        int y=(view_==View::profiles?136:152)+int(i-first)*16;
        if(i==dialog_selection_) fill(14,y/8,52,1,0x44e);
        text(120,y,items[i],i==dialog_selection_?15:0);
    }
    if(first) text(512,112,"^",0);
    if(first+8<items.size()) text(512,256,"v",0);
    text(120,list?264:208,u8"Enter - выбрать   Esc - назад",0);
}
void Trainer::text_input(const std::string& utf8) {
    if(!editing_name()) return;
    try {
        auto candidate=edit_name_+utf8;
        if(name_glyphs(candidate).size()>32) throw std::runtime_error("Name too long");
        edit_name_=candidate;dialog_message_.clear();
    } catch(const std::exception&) { dialog_message_=u8"Допустимы буквы, цифры, пробел, - и _"; }
    draw_dialog();
}
void Trainer::dialog_key(unsigned scan) {
    if(view_==View::message) {
        if(scan==1||scan==28||scan==100) {
            if(fatal_message_) running_=false;
            else { view_=View::main;draw_main(); }
        }
        return;
    }
    if(scan==1) {
        if(view_==View::error) action_={ActionKind::Cancel,{}};
        if(view_==View::edit) view_=creating_?View::profiles:View::profile_menu;
        else if(view_==View::profiles) view_=View::profile_menu;
        else view_=View::main;
        dialog_selection_=0;dialog_message_.clear();
        if(view_==View::main) draw_main();else draw_dialog();return;
    }
    if(view_==View::edit) {
        if(scan==14&&!edit_name_.empty()) {
            auto i=edit_name_.size()-1;while(i&&(static_cast<unsigned char>(edit_name_[i])&0xc0)==0x80) --i;
            edit_name_.erase(i);dialog_message_.clear();draw_dialog();
        }
        if(scan==28||scan==100) {
            try { edit_name_=normalize_name(edit_name_);action_={creating_?ActionKind::Create:ActionKind::Rename,edit_name_}; }
            catch(const std::exception&) { edit_error(u8"Введите имя от 1 до 32 символов"); }
        }
        return;
    }
    unsigned count=view_==View::profiles?unsigned(profiles_.size()+1):(view_==View::error&&closing_error_?3:2);
    if(scan==72||scan==95) dialog_selection_=(dialog_selection_+count-1)%count;
    if(scan==80||scan==96) dialog_selection_=(dialog_selection_+1)%count;
    if(scan==28||scan==100||scan==57) {
        if(view_==View::profile_menu) {
            if(dialog_selection_==0) { creating_=false;edit_name_=profile_.name;dialog_message_.clear();view_=View::edit; }
            else { action_={ActionKind::List,{}};return; }
        } else if(view_==View::profiles) {
            if(dialog_selection_==0) { creating_=true;edit_name_.clear();dialog_message_.clear();view_=View::edit; }
            else { action_={ActionKind::Switch,profiles_[dialog_selection_-1].id};return; }
        } else if(view_==View::error) {
            action_={dialog_selection_==0?ActionKind::Retry:(dialog_selection_==1?ActionKind::Cancel:ActionKind::Discard),{}};
            if(action_.kind==ActionKind::Cancel) { view_=View::main;draw_main();return; }
        }
    }
    draw_dialog();
}
std::string Trainer::diagnostic() const {
    std::ostringstream out;
    out<<"{\"line\":"<<line_<<",\"position\":"<<position_<<",\"flags\":"<<flags_
       <<",\"menu\":"<<(menu_?"true":"false")<<",\"selection\":"<<selection_
       <<",\"rhythm\":"<<rhythm_<<",\"count\":"<<total_count_<<",\"keys\":{";
    bool first=true;
    for(unsigned c=0;c<counts_.size();++c) {
        auto value=counts_[c];
        if(!value.correct&&!value.errors) continue;
        if(!first) out<<',';
        first=false;
        out<<'"'<<c<<"\":["<<value.ticks<<','<<value.correct<<','<<value.errors<<']';
    }
    out<<"}}";
    return out.str();
}
}
