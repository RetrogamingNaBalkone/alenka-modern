#pragma once
#ifdef _WIN32
#include <SDL_system.h>
#include <SDL_syswm.h>
#include <fstream>
#include <stdexcept>

// SDL2 derives physical keys from WM_KEY*'s scan field. Some Windows input
// services emit virtual-key messages with a zero scan field, which SDL2 drops.
// Supply only that missing case; physical keyboard messages remain SDL-owned.
class WindowsInput {
    HWND hwnd_ = nullptr;
    Uint32 window_id_ = 0;
    SDL_Keymod modifiers_ = KMOD_NONE;
    std::ofstream* log_ = nullptr;
    unsigned (*original_scan_)(SDL_Scancode) = nullptr;
    static void SDLCALL message(void* context,void* hwnd,unsigned int message,Uint64 vk,Sint64 parameter) {
        auto& self=*static_cast<WindowsInput*>(context);
        if(hwnd!=self.hwnd_) return;
        if(message==WM_KILLFOCUS) self.modifiers_=KMOD_NONE;
        if(message!=WM_KEYDOWN&&message!=WM_KEYUP&&message!=WM_SYSKEYDOWN&&message!=WM_SYSKEYUP) return;
        if(self.log_&&self.log_->is_open()) {
            *self.log_<<"WM "<<message<<' '<<vk<<' '<<parameter<<'\n';self.log_->flush();
        }
        if((parameter>>16)&255) return;
        unsigned raw=MapVirtualKeyW(UINT(vk),MAPVK_VK_TO_VSC_EX);
        bool extended=(raw&0xff00)==0xe000 || (parameter&(1ll<<24));
        unsigned scan=raw&255;
        if(extended) {
            switch(scan) {
            case 28: scan=100;break; case 29: scan=86;break;case 56:scan=85;break;
            case 71: scan=93;break;case 72:scan=95;break;case 73:scan=97;break;
            case 75:scan=92;break;case 77:scan=99;break;case 79:scan=94;break;
            case 80:scan=96;break;case 81:scan=98;break;case 82:scan=90;break;
            case 83:scan=91;break;case 53:scan=101;break;case 55:scan=89;break;
            default:scan=0;break;
            }
        } else if(scan==86) scan=0;
        if(vk==VK_NUMLOCK) scan=69;
        if(vk==VK_PAUSE) scan=102;
        if(vk==VK_SNAPSHOT) scan=89;
        SDL_Scancode code=SDL_SCANCODE_UNKNOWN;
        for(unsigned candidate=1;scan&&candidate<SDL_NUM_SCANCODES;++candidate)
            if(self.original_scan_(SDL_Scancode(candidate))==scan) { code=SDL_Scancode(candidate);break; }
        if(code==SDL_SCANCODE_UNKNOWN) return;
        const bool pressed=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;
        unsigned bit=0;
        switch(code) {
        case SDL_SCANCODE_LALT:bit=KMOD_LALT;break;case SDL_SCANCODE_RALT:bit=KMOD_RALT;break;
        case SDL_SCANCODE_LCTRL:bit=KMOD_LCTRL;break;case SDL_SCANCODE_RCTRL:bit=KMOD_RCTRL;break;
        case SDL_SCANCODE_LSHIFT:bit=KMOD_LSHIFT;break;case SDL_SCANCODE_RSHIFT:bit=KMOD_RSHIFT;break;
        default:break;
        }
        self.modifiers_=SDL_Keymod(pressed?(self.modifiers_|bit):(self.modifiers_&~bit));
        SDL_Event event{};
        event.type=pressed?SDL_KEYDOWN:SDL_KEYUP;
        event.key.timestamp=SDL_GetTicks();
        event.key.windowID=self.window_id_;
        event.key.state=pressed?SDL_PRESSED:SDL_RELEASED;
        event.key.repeat=pressed&&((parameter&(1ll<<30))!=0);
        event.key.keysym.scancode=code;
        event.key.keysym.sym=SDL_GetKeyFromScancode(code);
        event.key.keysym.mod=Uint16(SDL_GetModState()|self.modifiers_);
        SDL_PushEvent(&event);
    }
public:
    WindowsInput(SDL_Window* window,unsigned (*scan)(SDL_Scancode),std::ofstream* log):
        window_id_(SDL_GetWindowID(window)),log_(log),original_scan_(scan) {
        SDL_SysWMinfo info{};
        SDL_VERSION(&info.version);
        if(!SDL_GetWindowWMInfo(window,&info)) throw std::runtime_error(SDL_GetError());
        hwnd_=info.info.win.window;
        SDL_SetWindowsMessageHook(message,this);
    }
    ~WindowsInput() { SDL_SetWindowsMessageHook(nullptr,nullptr); }
    WindowsInput(const WindowsInput&)=delete;
    WindowsInput& operator=(const WindowsInput&)=delete;
};
#endif
