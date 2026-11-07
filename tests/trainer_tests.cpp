#include "trainer.hpp"
#include "sound.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace alenka {
struct TrainerFixture {
    static void select(Trainer& t,unsigned field) { t.selection_=field;t.draw_menu(true); }
    static const auto& lines(const Trainer& trainer) { return trainer.lines_; }
    static unsigned find(const Trainer& trainer,unsigned c,unsigned mods) { return trainer.find_key(c,mods); }
    static unsigned mods(const Trainer& trainer) { return trainer.modifiers(); }
    static unsigned errors(const Trainer& trainer,unsigned c) { return trainer.counts_[c].errors; }
    static unsigned total(const Trainer& trainer) { return trainer.total_count_; }
    static unsigned ghost(const Trainer& trainer) { return trainer.ghost_; }
    static bool russian(const Trainer& trainer) { return trainer.russian_; }
    static auto settings(const Trainer& t) {
        return std::array<unsigned,5>{t.flags_,t.hint_,t.rhythm_,unsigned(t.caps_),unsigned(t.sound_enabled_)};
    }
    static bool reset(const Trainer& t) {
        bool clean=t.line_==0&&t.position_==0&&t.ghost_==0&&t.menu_&&
            t.total_count_==0&&t.total_ticks_==0&&t.current_count_==0&&t.current_ticks_==0&&
            t.last_key_==0&&t.deadline_==0&&t.repeat_deadline_==0&&t.repeat_scan_==0&&
            !t.sounds_.beat&&!t.sounds_.error;
        for(const auto& count:t.counts_) clean=clean&&!count.ticks&&!count.correct&&!count.errors;
        return clean;
    }
    static void dirty(Trainer& t) {
        t.line_=unsigned(t.lines_.size()-1);t.position_=2;t.ghost_=3;
        t.total_count_=11;t.total_ticks_=22;t.current_count_=4;t.current_ticks_=5;
        t.counts_[65]={17,6,2};t.last_key_=9;t.deadline_=10;t.repeat_deadline_=11;
        t.sounds_={true,true};
    }
};
}
void expect(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
std::uint64_t time_for_tick(unsigned tick) { return (std::uint64_t(tick)*16384000+1193181)/1193182; }
void tap(alenka::Trainer& trainer,unsigned scan,std::uint64_t& time) {
    trainer.key(scan,true,++time);trainer.key(scan,false,++time);
}

void course(alenka::Trainer& trainer) {
    using Access=alenka::TrainerFixture;
        std::uint64_t time=100;
        unsigned typed=0;
        for(unsigned lesson=0;lesson<Access::lines(trainer).size();++lesson) {
            expect(trainer.line()==lesson && trainer.menu(),"Course line order changed");
            tap(trainer,28,time);
            for(unsigned c:Access::lines(trainer)[lesson]) {
                unsigned mods=Access::mods(trainer)&0x40;
                unsigned scan=Access::find(trainer,c,mods);
                bool shift=false;
                if(!scan) { scan=Access::find(trainer,c,mods|2);shift=scan!=0; }
                if(!scan) {
                    tap(trainer,58,time);mods^=0x40;
                    scan=Access::find(trainer,c,mods);
                    if(!scan) { scan=Access::find(trainer,c,mods|2);shift=scan!=0; }
                }
                if(!scan) {
                    // Original language switch is both physical Shift keys.
                    trainer.key(42,true,++time);trainer.key(54,true,++time);
                    trainer.key(54,false,++time);trainer.key(42,false,++time);
                    scan=Access::find(trainer,c,mods);
                    if(!scan) { scan=Access::find(trainer,c,mods|2);shift=scan!=0; }
                    if(!scan) {
                        tap(trainer,58,time);mods^=0x40;
                        scan=Access::find(trainer,c,mods);
                        if(!scan) { scan=Access::find(trainer,c,mods|2);shift=scan!=0; }
                    }
                }
                if(!scan) throw std::runtime_error("Original lesson character cannot be entered: line "+
                    std::to_string(lesson)+", byte "+std::to_string(c));
                trainer.key(42,shift,++time);
                tap(trainer,scan,time);
                trainer.key(42,false,++time);
                ++typed;
                // Rendering the entire course also checks every exercised glyph
                // and original blit address, including lowercase and punctuation.
                trainer.frame();
            }
            expect(trainer.menu(),"Completed line did not return to menu");
        }
        expect(Access::total(trainer)==typed,"Course lost or duplicated input");
        std::cout<<Access::lines(trainer).size()<<" original course lines / "<<typed<<" characters passed\n";

}
int main(int argc,char** argv) {
    try {
        expect(argc==2,"Usage: trainer-tests DATA");
        const alenka::Original original(std::filesystem::u8path(argv[1]));
        using alenka::Trainer;using Access=alenka::TrainerFixture;
        Trainer trainer(original,alenka::Screen::main);course(trainer);
        auto russian=trainer.profile().courses[0];std::uint64_t time=0;
        tap(trainer,99,time);tap(trainer,95,time);
        expect(trainer.course_language()==alenka::CourseLanguage::English&&trainer.line()==0,"English switch failed");
        course(trainer);expect(trainer.profile().courses[1].total_count==8683,"English count changed");
        tap(trainer,95,time);
        expect(trainer.profile().courses[0].total_count==russian.total_count&&trainer.line()==russian.line,"Russian state was lost");
        {
            Trainer t(original,alenka::Screen::main);time=0;
            tap(t,28,time);expect(t.take_sounds().beat,"Start lost beat");
            tap(t,32,time);expect(t.position()==0&&t.take_sounds().error,"Wrong-key rejection changed");
            tap(t,37,time);expect(t.position()==1&&!t.take_sounds().error,"Correct key failed");
            t.advance(time_for_tick(145));expect(!t.take_sounds().beat,"Early beat");
            t.advance(time_for_tick(146));expect(t.take_sounds().beat,"Missing beat");
            time=time_for_tick(146);tap(t,1,time);expect(t.menu(),"Esc did not leave input");
            tap(t,1,time);tap(t,1,time);expect(t.running()&&t.menu(),"Esc exited application");
            tap(t,59,time);tap(t,30,time);tap(t,28,time);tap(t,1,time);expect(t.menu(),"Help Esc failed");
        }
        for(unsigned mode:{3u,4u}) {
            Trainer t(original,alenka::Screen::main);time=0;
            for(unsigned i=0;i<mode;++i) tap(t,99,time);
            tap(t,95,time);tap(t,28,time);
            if(mode==3) tap(t,32,time);else t.advance(time_for_tick(146));
            expect(t.position()==1&&Access::errors(t,139)==1,"Typo or paced mode failed");
        }
        {
            Trainer t(original,alenka::Screen::main);time=0;
            Access::select(t,7);
            t.key(96,true,++time);t.advance(time_for_tick(27));expect(t.line()==5,"Line autorepeat changed");
            t.key(96,false,time_for_tick(27));t.advance(time_for_tick(100));expect(t.line()==5,"Released key repeats");
            t.key(96,true,time_for_tick(101));t.release_keys();t.advance(time_for_tick(200));expect(t.line()==6,"Blur did not stop repeat");
        }
        for(bool error:{false,true}) {
            const auto samples=alenka::sound_samples(error);unsigned peak=0;
            for(auto n:samples) peak=std::max(peak,unsigned(std::abs(int(n))));
            expect(samples.size()==(error?3840u:720u)&&samples.front()==0&&samples.back()==0&&peak>1000&&peak<=6000,"PCM changed");
        }
        std::cout<<"Trainer, audio events and both full courses passed\n";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
