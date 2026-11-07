#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "original.hpp"
#include "trainer.hpp"
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <SDL_syswm.h>
#include "windows_input.hpp"
#endif

namespace {
void check(int result) { if (result < 0) throw std::runtime_error(SDL_GetError()); }
template<class T> T* checked(T* value) {
    if (!value) throw std::runtime_error(SDL_GetError());
    return value;
}
struct SDLGuard {
    SDLGuard() {
        SDL_SetMainReady();
#ifdef _WIN32
        // The embedded manifest sets this before process startup. Also request
        // it through SDL for development builds launched without that resource.
        SDL_SetHintWithPriority(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2", SDL_HINT_OVERRIDE);
#endif
        check(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
    }
    ~SDLGuard() { SDL_ShowCursor(SDL_ENABLE); SDL_Quit(); }
};
struct Options {
    std::filesystem::path data, capture, input_log;
    alenka::Screen screen = alenka::Screen::intro;
    int width = 960, height = 720;
    bool fullscreen = false, smoke = false;
};
Options parse(int argc, char** argv) {
    Options options;
    auto next = [&](int& i) -> std::string {
        if (++i >= argc) throw std::runtime_error("Missing option argument");
        return argv[i];
    };
    for (int i=1; i<argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--data") options.data = std::filesystem::u8path(next(i));
        else if (arg == "--capture") options.capture = std::filesystem::u8path(next(i));
        else if (arg == "--input-log") options.input_log = std::filesystem::u8path(next(i));
        else if (arg == "--fullscreen") options.fullscreen = true;
        else if (arg == "--smoke-test") options.smoke = true;
        else if (arg == "--screen") {
            auto value = next(i);
            if (value == "intro") options.screen = alenka::Screen::intro;
            else if (value == "help") options.screen = alenka::Screen::help;
            else if (value == "main") options.screen = alenka::Screen::main;
            else throw std::runtime_error("Unknown screen: " + value);
        } else if (arg == "--size") {
            auto value = next(i);
            auto separator = value.find('x');
            if (separator == std::string::npos) throw std::runtime_error("Use --size WIDTHxHEIGHT");
            auto number = [](const std::string& s) {
                std::size_t end;
                int v = std::stoi(s, &end);
                if (end != s.size() || v < 1 || v > 16384) throw std::runtime_error("Invalid window size");
                return v;
            };
            options.width = number(value.substr(0, separator));
            options.height = number(value.substr(separator+1));
        } else throw std::runtime_error("Unknown option: " + arg);
    }
    if (options.data.empty()) {
        char* path = checked(SDL_GetBasePath());
        options.data = std::filesystem::u8path(path) / "data";
        SDL_free(path);
    }
#ifdef ALENKA_INPUT_DIAGNOSTICS
    if(options.input_log.empty()) {
        char* path=checked(SDL_GetBasePath());
        options.input_log=std::filesystem::u8path(path)/"input.log";
        SDL_free(path);
    }
#endif
    return options;
}
void cursor(bool fullscreen, bool focused) {
    check(SDL_ShowCursor(fullscreen && focused ? SDL_DISABLE : SDL_ENABLE));
}
bool fullscreen(SDL_Window* window) {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}
void toggle_fullscreen(SDL_Window* window) {
    check(SDL_SetWindowFullscreen(window, fullscreen(window) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP));
    cursor(fullscreen(window), (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0);
}
unsigned original_scan(SDL_Scancode scan) {
    static constexpr SDL_Scancode set1[] = {
        SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_ESCAPE,
        SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_3,SDL_SCANCODE_4,SDL_SCANCODE_5,SDL_SCANCODE_6,
        SDL_SCANCODE_7,SDL_SCANCODE_8,SDL_SCANCODE_9,SDL_SCANCODE_0,SDL_SCANCODE_MINUS,SDL_SCANCODE_EQUALS,
        SDL_SCANCODE_BACKSPACE,SDL_SCANCODE_TAB,SDL_SCANCODE_Q,SDL_SCANCODE_W,SDL_SCANCODE_E,SDL_SCANCODE_R,
        SDL_SCANCODE_T,SDL_SCANCODE_Y,SDL_SCANCODE_U,SDL_SCANCODE_I,SDL_SCANCODE_O,SDL_SCANCODE_P,
        SDL_SCANCODE_LEFTBRACKET,SDL_SCANCODE_RIGHTBRACKET,SDL_SCANCODE_RETURN,SDL_SCANCODE_LCTRL,
        SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_F,SDL_SCANCODE_G,SDL_SCANCODE_H,
        SDL_SCANCODE_J,SDL_SCANCODE_K,SDL_SCANCODE_L,SDL_SCANCODE_SEMICOLON,SDL_SCANCODE_APOSTROPHE,
        SDL_SCANCODE_GRAVE,SDL_SCANCODE_LSHIFT,SDL_SCANCODE_BACKSLASH,SDL_SCANCODE_Z,SDL_SCANCODE_X,
        SDL_SCANCODE_C,SDL_SCANCODE_V,SDL_SCANCODE_B,SDL_SCANCODE_N,SDL_SCANCODE_M,SDL_SCANCODE_COMMA,
        SDL_SCANCODE_PERIOD,SDL_SCANCODE_SLASH,SDL_SCANCODE_RSHIFT,SDL_SCANCODE_KP_MULTIPLY,
        SDL_SCANCODE_LALT,SDL_SCANCODE_SPACE,SDL_SCANCODE_CAPSLOCK,
        SDL_SCANCODE_F1,SDL_SCANCODE_F2,SDL_SCANCODE_F3,SDL_SCANCODE_F4,SDL_SCANCODE_F5,
        SDL_SCANCODE_F6,SDL_SCANCODE_F7,SDL_SCANCODE_F8,SDL_SCANCODE_F9,SDL_SCANCODE_F10,
        SDL_SCANCODE_NUMLOCKCLEAR,SDL_SCANCODE_SCROLLLOCK,
        SDL_SCANCODE_KP_7,SDL_SCANCODE_KP_8,SDL_SCANCODE_KP_9,SDL_SCANCODE_KP_MINUS,
        SDL_SCANCODE_KP_4,SDL_SCANCODE_KP_5,SDL_SCANCODE_KP_6,SDL_SCANCODE_KP_PLUS,
        SDL_SCANCODE_KP_1,SDL_SCANCODE_KP_2,SDL_SCANCODE_KP_3,SDL_SCANCODE_KP_0,SDL_SCANCODE_KP_PERIOD,
        SDL_SCANCODE_SYSREQ,SDL_SCANCODE_RALT,SDL_SCANCODE_RCTRL,SDL_SCANCODE_F11,SDL_SCANCODE_F12,
        SDL_SCANCODE_PRINTSCREEN,SDL_SCANCODE_INSERT,SDL_SCANCODE_DELETE,SDL_SCANCODE_LEFT,
        SDL_SCANCODE_HOME,SDL_SCANCODE_END,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,
        SDL_SCANCODE_PAGEUP,SDL_SCANCODE_PAGEDOWN,SDL_SCANCODE_RIGHT,SDL_SCANCODE_KP_ENTER,
        SDL_SCANCODE_KP_DIVIDE,SDL_SCANCODE_PAUSE
    };
    for(unsigned i=1;i<sizeof(set1)/sizeof(set1[0]);++i) if(set1[i]==scan) return i;
    return 0;
}
class KeyboardRouter {
    SDL_Window* window_;
    alenka::Trainer& trainer_;
    unsigned pending_alt_=0;
    bool shortcut_=false;
public:
    KeyboardRouter(SDL_Window* window,alenka::Trainer& trainer):window_(window),trainer_(trainer) {}
    void blur() { pending_alt_=0;shortcut_=false;trainer_.release_keys(); }
    void key(const SDL_KeyboardEvent& event,std::uint64_t now,bool& running) {
        if(event.repeat) return;
        unsigned scan=original_scan(event.keysym.scancode);
        bool down=event.type==SDL_KEYDOWN;
        if(scan==56||scan==85) {
            if(down) {
                if(pending_alt_&&pending_alt_!=scan) trainer_.key(pending_alt_,true,now);
                pending_alt_=scan;
            } else {
                if(pending_alt_==scan&&!shortcut_) trainer_.key(scan,true,now);
                pending_alt_=0;shortcut_=false;
                trainer_.key(scan,false,now);
            }
            return;
        }
        bool shell_key=event.keysym.sym==SDLK_RETURN||event.keysym.sym==SDLK_F4;
        if(down&&shell_key&&(event.keysym.mod&KMOD_ALT)) {
            pending_alt_=0;shortcut_=true;
            if(event.keysym.sym==SDLK_RETURN) toggle_fullscreen(window_);
            else running=false;
            return;
        }
        if(!down&&shell_key&&shortcut_) return;
        if(pending_alt_) { trainer_.key(pending_alt_,true,now);pending_alt_=0; }
        trainer_.key(scan,down,now);
    }
};
void capture(SDL_Renderer* renderer, const std::filesystem::path& path) {
    int w, h;
    check(SDL_GetRendererOutputSize(renderer, &w, &h));
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
        checked(SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888)), SDL_FreeSurface);
    check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch));
    check(SDL_SaveBMP(surface.get(), path.u8string().c_str()));
}
void smoke(SDL_Window* window,const alenka::Original& original) {
#ifdef _WIN32
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) throw std::runtime_error(SDL_GetError());
    const auto user32 = GetModuleHandleW(L"user32.dll");
    const auto get_context = reinterpret_cast<HANDLE (WINAPI*)(HWND)>(
        GetProcAddress(user32, "GetWindowDpiAwarenessContext"));
    const auto equal_context = reinterpret_cast<BOOL (WINAPI*)(HANDLE,HANDLE)>(
        GetProcAddress(user32, "AreDpiAwarenessContextsEqual"));
    if (!get_context || !equal_context ||
        !equal_context(get_context(info.info.win.window), reinterpret_cast<HANDLE>(-4)))
        throw std::runtime_error("Window is not Per-Monitor V2 DPI aware");
    std::cout << "Windows Per-Monitor V2 DPI awareness verified\n";
    if(!PostMessageW(info.info.win.window,WM_KEYDOWN,VK_RETURN,1) ||
       !PostMessageW(info.info.win.window,WM_KEYUP,VK_RETURN,LPARAM(0xc0000001u)))
        throw std::runtime_error("Cannot queue Windows virtual-key input");
    bool key_down=false,key_up=false;
    SDL_Event injected;
    for(unsigned attempt=0;attempt<5&&(!key_down||!key_up);++attempt) {
        SDL_PumpEvents();
        while(SDL_PollEvent(&injected)) {
            if(injected.type==SDL_KEYDOWN&&original_scan(injected.key.keysym.scancode)==28) key_down=true;
            if(injected.type==SDL_KEYUP&&original_scan(injected.key.keysym.scancode)==28) key_up=true;
        }
    }
    if(!key_down||!key_up) throw std::runtime_error("Windows virtual-key input fallback failed");
    std::cout << "Windows virtual-key input fallback verified\n";
#endif
    {
        alenka::Trainer trainer(original,alenka::Screen::main);
        bool running=true;
        trainer.key(99,true,0);trainer.key(99,false,0);
        trainer.key(95,true,0);trainer.key(95,false,0); // Enable typo mode.
        trainer.key(37,true,0);trainer.key(37,false,0); // Begin typing.
        KeyboardRouter router(window,trainer);
        for(unsigned repeat=0;repeat<2;++repeat) {
            SDL_KeyboardEvent key{};key.type=SDL_KEYDOWN;key.keysym.mod=KMOD_LALT;
            key.keysym.scancode=SDL_SCANCODE_LALT;key.keysym.sym=SDLK_LALT;router.key(key,0,running);
            key.keysym.scancode=SDL_SCANCODE_RETURN;key.keysym.sym=SDLK_RETURN;router.key(key,0,running);
            key.type=SDL_KEYUP;router.key(key,0,running);
            key.keysym.scancode=SDL_SCANCODE_LALT;key.keysym.sym=SDLK_LALT;key.keysym.mod=0;router.key(key,0,running);
        }
        if(!running||trainer.position()!=1) throw std::runtime_error("Alt+Enter was consumed as exercise input");
        std::cout<<"Fullscreen shortcuts leave exercise input unchanged\n";
    }
    // Real SDL window transitions, not a mock of SDL's flags or cursor state.
    if (fullscreen(window)) toggle_fullscreen(window);
    toggle_fullscreen(window);
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN_DESKTOP)
        throw std::runtime_error("Borderless fullscreen desktop transition failed");
    cursor(true, true);
    if (SDL_ShowCursor(SDL_QUERY) != SDL_DISABLE) throw std::runtime_error("Fullscreen cursor is visible");
    cursor(true, false);
    if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE) throw std::runtime_error("Unfocused cursor is hidden");
    toggle_fullscreen(window);
    if (fullscreen(window) || SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
        throw std::runtime_error("Windowed mode was not restored");
    std::cout << "SDL fullscreen/window/cursor smoke passed\n";
}
}

int main(int argc, char** argv) {
    try {
        SDLGuard sdl;
        const auto options = parse(argc, argv);
        const alenka::Original original(options.data);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
        if (options.smoke || !options.capture.empty()) flags |= SDL_WINDOW_HIDDEN;
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            checked(SDL_CreateWindow("\xD0\x90\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBA\xD0\xB0", SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED, options.width, options.height, flags)), SDL_DestroyWindow);
        SDL_Renderer* raw = SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!raw) raw = SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE);
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(checked(raw), SDL_DestroyRenderer);
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
            checked(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                alenka::frame_width, alenka::frame_height)), SDL_DestroyTexture);
        if (options.fullscreen) toggle_fullscreen(window.get());
        alenka::Trainer trainer(original,options.screen);
        KeyboardRouter keyboard(window.get(),trainer);
        std::ofstream input_log;
        if(!options.input_log.empty()) {
            input_log.open(options.input_log);
            if(!input_log) throw std::runtime_error("Cannot create input diagnostic log");
            input_log<<"SDL input diagnostics "<<SDL_GetCurrentVideoDriver()<<'\n';input_log.flush();
        }
#ifdef _WIN32
        WindowsInput windows_input(window.get(),original_scan,&input_log);
#endif
        bool running = true, dirty = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_WINDOWEVENT &&
                    (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || event.window.event == SDL_WINDOWEVENT_FOCUS_LOST))
                {
                    bool focused=event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED;
                    cursor(fullscreen(window.get()),focused);
                    if(!focused) keyboard.blur();
                }
                if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) continue;
                if(input_log.is_open()) {
                    input_log<<event.type<<' '<<event.key.keysym.scancode<<' '<<event.key.keysym.sym<<' '
                        <<event.key.keysym.mod<<' '<<unsigned(event.key.repeat)<<' '
                        <<original_scan(event.key.keysym.scancode)<<'\n';
                    input_log.flush();
                }
                keyboard.key(event.key,SDL_GetTicks64(),running);
                dirty=true;
            }
            trainer.advance(SDL_GetTicks64());
            dirty=true;
            if (!running || !trainer.running()) break;
            if (dirty) {
                const auto frame = trainer.frame();
                check(SDL_UpdateTexture(texture.get(), nullptr, frame.data(), alenka::frame_width*4));
                dirty = false;
            }
            const auto color = original.border_color();
            check(SDL_SetRenderDrawColor(renderer.get(), Uint8(color>>16), Uint8(color>>8), Uint8(color), 255));
            check(SDL_RenderClear(renderer.get()));
            int width, height;
            check(SDL_GetRendererOutputSize(renderer.get(), &width, &height));
            auto fit = alenka::fit_display(width, height);
            SDL_Rect destination{fit.x, fit.y, fit.w, fit.h};
            if (fit.w && fit.h) check(SDL_RenderCopy(renderer.get(), texture.get(), nullptr, &destination));
            if (!options.capture.empty()) capture(renderer.get(), options.capture);
            SDL_RenderPresent(renderer.get());
            if (options.smoke) smoke(window.get(),original);
            if (options.smoke || !options.capture.empty()) break;
            SDL_Delay(8);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Alenka: " << error.what() << '\n';
        return 1;
    }
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int) { return main(__argc,__argv); }
#endif
