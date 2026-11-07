#include "trainer.hpp"
#include <iostream>
#include <stdexcept>

namespace alenka {
struct TrainerFixture {
    static const auto& lines(const Trainer& trainer) { return trainer.lines_; }
    static unsigned find(const Trainer& trainer,unsigned c,unsigned mods) { return trainer.find_key(c,mods); }
    static unsigned mods(const Trainer& trainer) { return trainer.modifiers(); }
    static unsigned errors(const Trainer& trainer,unsigned c) { return trainer.counts_[c].errors; }
    static unsigned total(const Trainer& trainer) { return trainer.total_count_; }
    static unsigned ghost(const Trainer& trainer) { return trainer.ghost_; }
};
}
void expect(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
std::uint64_t time_for_tick(unsigned tick) { return (std::uint64_t(tick)*16384000+1193181)/1193182; }
void tap(alenka::Trainer& trainer,unsigned scan,std::uint64_t& time) {
    trainer.key(scan,true,++time);trainer.key(scan,false,++time);
}
int main(int argc,char** argv) {
    try {
        expect(argc==2,"Usage: trainer-tests DATA");
        const alenka::Original original(std::filesystem::u8path(argv[1]));
        using alenka::Trainer; using Access=alenka::TrainerFixture;
        Trainer trainer(original,alenka::Screen::main);
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
        {
            Trainer ignored(original,alenka::Screen::main);time=0;
            tap(ignored,28,time);tap(ignored,32,time);
            expect(ignored.position()==0 && !ignored.menu(),"Default wrong-key rejection changed");
        }
        {
            Trainer errors(original,alenka::Screen::main);time=0;
            tap(errors,99,time);tap(errors,95,time);tap(errors,28,time);tap(errors,32,time);
            expect(errors.position()==1 && Access::errors(errors,139)==1,"Typo mode did not record wrong input");
        }
        {
            Trainer paced(original,alenka::Screen::main);time=0;
            tap(paced,99,time);tap(paced,99,time);tap(paced,95,time);tap(paced,28,time);
            paced.advance(time_for_tick(145));expect(paced.position()==0,"Paced timeout fired early");
            paced.advance(time_for_tick(146));
            expect(paced.position()==1 && Access::errors(paced,139)==1,"Paced timeout did not consume the missed character");
        }
        {
            Trainer unpaced(original,alenka::Screen::main);time=0;tap(unpaced,28,time);
            unpaced.advance(time_for_tick(146));
            expect(unpaced.position()==0 && Access::ghost(unpaced)==1,"Unpaced rhythm cursor consumed input");
        }
        {
            Trainer held(original,alenka::Screen::main);time=0;
            for(unsigned i=0;i<5;++i) tap(held,99,time);
            held.key(96,true,++time);
            held.advance(time_for_tick(27));
            expect(held.line()==5,"Original 24/1 tick line autorepeat changed");
            held.key(96,false,time_for_tick(27));held.advance(time_for_tick(100));
            expect(held.line()==5,"Released navigation key continues repeating");
            held.key(96,true,time_for_tick(101));held.release_keys();held.advance(time_for_tick(200));
            expect(held.line()==6,"Focus loss did not stop held navigation");
        }
        std::cout<<"Input, error handling, timers and held-key checks passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
