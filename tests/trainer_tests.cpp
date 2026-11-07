#include "trainer.hpp"
#include "sound.hpp"
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
        {
            Trainer sound(original,alenka::Screen::main); std::uint64_t clock=0;
            expect(sound.sound_enabled(),"Sound must start enabled");
            sound.advance(100); expect(!sound.take_sounds().beat,"Menu emitted metronome");
            tap(sound,28,clock); expect(sound.take_sounds().beat,"Exercise start lost beat");
            tap(sound,32,clock);
            expect(sound.take_sounds().error&&sound.position()==0,"Rejected typo must sound without consuming input");
            expect(!sound.take_sounds().error,"Error cue was not drained");
            tap(sound,37,clock); expect(!sound.take_sounds().error,"Correct key sounded as error");
            sound.advance(time_for_tick(145)); expect(!sound.take_sounds().beat,"Metronome fired early");
            sound.advance(time_for_tick(146)); expect(sound.take_sounds().beat,"Metronome missed original rhythm deadline");
            sound.advance(time_for_tick(1000));
            expect(sound.take_sounds().beat&&!sound.take_sounds().beat,"Catch-up must coalesce beats");
        }
        {
            Trainer sound(original,alenka::Screen::main); std::uint64_t clock=0;
            tap(sound,1,clock); tap(sound,99,clock); tap(sound,99,clock); // Speaker.
            const auto enabled_frame=sound.frame();
            tap(sound,57,clock);
            expect(!sound.sound_enabled()&&sound.frame()!=enabled_frame,"Speaker must display muted state");
            tap(sound,28,clock); tap(sound,28,clock); tap(sound,32,clock);
            sound.advance(time_for_tick(146));
            auto cues=sound.take_sounds();
            expect(!cues.error&&!cues.beat,"Muted trainer emitted audio");
            clock=time_for_tick(146);
            tap(sound,1,clock); tap(sound,99,clock); tap(sound,99,clock); tap(sound,57,clock);
            expect(sound.sound_enabled()&&Access::total(sound)==0,"Unmute changed input statistics");
        }
        {
            Trainer sound(original,alenka::Screen::main); std::uint64_t clock=0;
            tap(sound,99,clock); tap(sound,99,clock); tap(sound,95,clock); tap(sound,28,clock);
            sound.take_sounds(); sound.advance(time_for_tick(146));
            expect(sound.take_sounds().error,"Missed paced character must sound as error");
        }
        {
            Trainer sound(original,alenka::Screen::main); std::uint64_t clock=0;
            for(unsigned i=0;i<6;++i) tap(sound,99,clock);
            for(unsigned i=0;i<37;++i) tap(sound,95,clock); // Rhythm 400.
            tap(sound,28,clock); sound.take_sounds();
            // Deadline is based on the actual start tick, with integer 4380/rhythm.
            unsigned start=unsigned((clock-1)*1193182/16384000);
            sound.advance(time_for_tick(start+9)); expect(!sound.take_sounds().beat,"Fast rhythm fired early");
            sound.advance(time_for_tick(start+10)); expect(sound.take_sounds().beat,"Rhythm setting did not change beat interval");
            clock=time_for_tick(start+10);
            tap(sound,59,clock); sound.take_sounds(); sound.advance(time_for_tick(start+100));
            expect(!sound.take_sounds().beat,"Help emitted metronome");
        }
        for(bool error:{false,true}) {
            const auto samples=alenka::sound_samples(error);
            expect(samples.size()==(error?3840u:720u),"Cue duration changed");
            expect(samples.front()==0&&samples.back()==0,"Cue lacks smooth endpoints");
            unsigned peak=0;
            for(auto sample:samples) peak=std::max(peak,unsigned(std::abs(int(sample))));
            expect(peak>1000&&peak<=6000,"Cue is silent or clips");
        }
        std::cout<<"Sound events, mute, rhythm and synthesized PCM checks passed\n";
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
