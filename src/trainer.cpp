#include "trainer.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace alenka {
Trainer::Trainer(const Original& original, Screen start) : original_(original), field_(original.field_) {
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
    if (start == Screen::main) { view_ = View::main; draw_main(); }
    else if (start == Screen::help) { view_ = View::help; blit(0x47e0,0,0,80,44); }
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
void Trainer::field_blit(int source,int destination,int w,int h) {
    auto previous=field_;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x)
        field_.at(destination+y*80+x)=previous.at(source+y*80+x);
}
void Trainer::number(int destination,unsigned width,std::uint32_t value,unsigned font,unsigned blank) {
    auto digits=std::to_string(value);
    for(unsigned i=0;i<width;++i) {
        int d=int(digits.size())-int(width)+int(i);
        field_.at(destination+i)=tile(d<0?blank:font+digits[std::size_t(d)]-'0');
    }
}
void Trainer::menu_number(unsigned selection,std::uint32_t value,unsigned width) {
    unsigned x=original_.data_[0xc22+selection*2];
    number(0x4330+x+162,width,value,0x23a,0x1a0);
    number(0x4470+x+162,width,value,0x230,0x1a0);
    number(0xead+x+162,width,value,0x4e7,0x44e);
}
void Trainer::menu_toggle(unsigned selection,bool enabled) {
    unsigned x=original_.data_[0xc22+selection*2];
    field_blit(enabled?0x4384:0x4334,0x4330+x+162,3,1);
    field_blit(enabled?0x4380:0x4330,0x4470+x+162,3,1);
    field_blit(enabled?0x4424:0x43d4,0xead+x+162,3,1);
}
void Trainer::draw_menu(bool active) {
    fill(4,2,71,6,0x44e);
    blit(active?0x4479:0xeb6,6,3,69,4);
    if (!active) return;
    unsigned x=original_.data_[0xc22+selection_*2], width=original_.data_[0xc23+selection_*2];
    blit(0x4330+x,int(x)-3,3,width,4);
    blit(0x4470,int(x+width/2)-4,2,3,1);
    blit(0x44c0,int(x+width/2)-4,7,3,1);
    blit(0x43d0,int(x)-4,4,1,2);
    blit(0x43d1,int(x+width)-3,4,1,2);
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
    help_return_=view_;
    view_=View::help;
    menu_=true;
    position_=0;
    blit(0x47e0,0,0,80,44);
}
void Trainer::show_statistics() {
    menu_=true;
    position_=0;
    view_=View::statistics;
    draw_statistics();
}
void Trainer::draw_stat_selection() {
    unsigned x=original_.data_[0xc30+stat_selection_*2], w=original_.data_[0xc31+stat_selection_*2];
    blit(0x4470,int(x+w/2),1,3,1);
    blit(0x44c0,int(x+w/2),6,3,1);
    blit(0x43d0,int(x)-1,3,1,2);
    blit(0x43d1,int(x+w)+1,3,1,2);
}
void Trainer::draw_statistics() {
    blit(0x3570,0,0,80,44);
    fill(0,18,37,21,0x44e); fill(42,18,37,21,0x44e);
    // Character columns are the original finger-group ordering in DS:02bb.
    unsigned max_average=0;
    for(unsigned i=0;i<44;++i) {
        const auto& count=counts_[character(original_.data_[0x2bb+i*2])];
        unsigned average=count.correct?std::uint16_t(count.ticks/count.correct):0;
        max_average=std::max(max_average,unsigned(std::uint16_t(average+count.errors)));
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
            bar(x,count.correct?std::uint16_t(count.ticks/count.correct)*80/max_average:0,
                  std::uint16_t(unsigned(count.errors)*80)/max_average);
    }
    draw_stat_selection();
}
void Trainer::update_setting(int direction,bool space) {
    switch(selection_) {
    case 1: flags_^=0x10; menu_toggle(1,(flags_&0x10)==0); break;
    case 2: flags_^=0x40; menu_toggle(2,(flags_&0x40)==0); break;
    case 3:
        if(space) return;
        hint_=(hint_+(direction>0?1:2))%3;
        flags_=(flags_&0xf0)|constant(0xc18+hint_*2);
        menu_number(3,hint_,2); draw_keyboard(); break;
    case 4: show_statistics(); return;
    case 5:
        if(space) return;
        choose_line(direction>0?-1:1); break;
    case 6:
        if(space) return;
        if(direction>0) rhythm_=std::min(400u,rhythm_+10);
        else rhythm_=rhythm_>10?rhythm_-10:10;
        menu_number(6,rhythm_,3); break;
    default: return;
    }
    draw_menu(true);
}
void Trainer::choose_line(int direction) {
    if(direction<0&&!line_) return;
    if(direction<0&&line_) --line_;
    else if(direction>0&&line_+1<lines_.size()) ++line_;
    position_=0;
    menu_number(5,line_,3);
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
    ++total_count_; total_ticks_+=ticks; ++current_count_; current_ticks_+=ticks;
    if(correct) { counts_[c].ticks+=ticks; ++counts_[c].correct; }
    else ++counts_[c].errors;
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
    move(5,12,5,11,70,4);
    if(completed&&line_+1<lines_.size()) ++line_;
    menu_number(5,line_,3);
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
        if(!pressed&&(scan==28||scan==57)) { view_=View::help; blit(0x47e0,0,0,80,44); }
        return;
    }
    if(view_==View::help||view_==View::unavailable) {
        if(pressed) {
            view_=help_return_;
            if(view_==View::statistics) draw_statistics(); else draw_main();
        }
        return;
    }
    if(pressed) {
        if(scan==58) caps_=!caps_;
        if(scan==69) num_=!num_;
        if(scan==70) scroll_=!scroll_;
        if((scan==42||scan==54)&&keys_[42]&&keys_[54]) russian_=!russian_;
    }
    if(view_==View::statistics) {
        if(!pressed) return;
        if(scan==1) { running_=false; return; }
        if(scan==59) { show_help(); return; }
        if(scan==28||scan==100) { view_=View::main; draw_main(); return; }
        if(scan==75||scan==92||scan==77||scan==99) {
            stat_selection_=(stat_selection_+((scan==75||scan==92)?6:1))%7;
            draw_statistics(); return;
        }
        if(scan==42||scan==54||scan==58) { draw_statistics(); return; }
        if(scan==57||scan==72||scan==80||scan==95||scan==96) {
            if(stat_selection_==0) { view_=View::main; draw_main(); }
            else if(stat_selection_==1) { counts_={}; draw_statistics(); }
            else if(stat_selection_==3) {
                sound_enabled_=!sound_enabled_;
                sounds_={};
                draw_statistics();
            }
            else {
                help_return_=View::statistics; view_=View::unavailable;
                blit(0x45bc,35,4,37,7);
            }
        }
        return;
    }
    draw_keyboard(); draw_key(scan); draw_locks();
    if(scan==28) blit(int(constant(pressed?0x92:0x90))+7*80+40,45,28,1,3);
    if(!pressed) return;
    if(scan==59) { show_help(); return; }
    if(scan==1) { stat_selection_=1; show_statistics(); return; }
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
            selection_=(selection_+((scan==75||scan==92)?6:1))%7;
            draw_menu(true); return;
        }
        if(scan==72||scan==95||scan==80||scan==96) {
            update_setting((scan==72||scan==95)?1:-1,false);
            if(selection_==5) { repeat_scan_=scan; repeat_deadline_=now_*1193182/16384000+24; }
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
    if(view_==View::statistics&&!sound_enabled_) {
        // Cross out the original speaker artwork inside its button. Draw at
        // native EGA resolution so the stroke scales with the original pixels.
        for(int x=270;x<=302;++x) {
            const int y=39-(x-270)/2;
            pixels[(y-1)*frame_width+x]=original_.palette_[15];
            pixels[y*frame_width+x]=original_.palette_[12];
            pixels[(y+1)*frame_width+x]=original_.palette_[12];
            pixels[(y+2)*frame_width+x]=original_.palette_[15];
        }
    }
    return pixels;
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
