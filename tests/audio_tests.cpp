#define SDL_MAIN_HANDLED
#include "audio.hpp"
#include <iostream>
#include <stdexcept>

void expect(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        SDL_SetMainReady();
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        {
            alenka::Audio audio;
            expect(audio.available(),"Dummy audio device failed");
            audio.update(true,false,true);
            expect(audio.queued_bytes()>0&&audio.queued_bytes()<=1440,"Beat not queued");
            audio.update(true,true,true);
            expect(audio.queued_bytes()>1440&&audio.queued_bytes()<=7680,"Error did not take priority");
            for(unsigned i=0;i<100;++i) audio.update(true,false,true);
            expect(audio.queued_bytes()<=7680,"Metronome built a backlog");
            audio.update(false,true,true); expect(audio.queued_bytes()==0,"Mute did not stop audio");
            audio.update(true,true,false); audio.stop(); expect(audio.queued_bytes()==0,"Focus stop did not clear audio");
            audio.update(true,false,true); SDL_Delay(120);
            expect(audio.queued_bytes()==0,"Queued PCM was not consumed by SDL");
        }
        SDL_Quit();
        SDL_setenv("SDL_AUDIODRIVER","alenka-nonexistent-device",1);
        {
            alenka::Audio audio;
            expect(!audio.available(),"Invalid audio driver unexpectedly opened");
            audio.update(true,true,true); audio.stop();
        }
        SDL_Quit();
        std::cout<<"SDL PCM playback, priority, bounded queue, mute and missing-device checks passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; SDL_Quit(); return 1; }
}
