#include "session.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace alenka {
struct TrainerFixture {
    static void select(Trainer& t,unsigned field) { t.selection_=field;t.draw_menu(true); }
    static unsigned expected(const Trainer& t) { return t.expected_key(); }
    static Frame letter(Trainer& t) { t.texts_.clear();t.text(80,80,"A",14);return t.frame(); }
    static bool original_buttons(const Trainer& t) {
        for(unsigned row=0;row<4;++row) for(unsigned x=0;x<8;++x)
            if(t.cells_[(row+2)*80+31+x]!=t.field_[0x3570+(row+2)*80+5+x]||
               t.cells_[(row+2)*80+41+x]!=t.field_[0x3570+(row+2)*80+14+x]) return false;
        return true;
    }
};
}
namespace {
namespace fs=std::filesystem;
using alenka::Trainer;
void expect(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f,const char* message) {
    bool rejected=false;try { f(); } catch(const std::exception&) { rejected=true; }expect(rejected,message);
}
std::uint64_t clock_ms=0;
void tap(Trainer& t,unsigned scan) { t.key(scan,true,++clock_ms);t.key(scan,false,++clock_ms); }
void setting(Trainer& t,unsigned field) { alenka::TrainerFixture::select(t,field);tap(t,95); }
std::uint64_t total(const Trainer& t,unsigned lang=0) { return t.profile().courses[lang].total_count; }
struct Temp {
    fs::path path=fs::current_path()/"build"/("profile-test-"+alenka::new_profile_id());
    Temp() { fs::create_directories(path); }
    ~Temp() { std::error_code ec;fs::remove_all(path,ec); }
};
void write(const fs::path& path,const std::string& s) { std::ofstream out(path,std::ios::binary);out<<s;expect(bool(out),"Cannot write fixture"); }
std::string read(const fs::path& path) { std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}}; }
void capture(const Trainer& t,const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    const unsigned char header[]={128,2,0,0,94,1,0,0,0,10,0,0,32,0,0};out.write(reinterpret_cast<const char*>(header),15);
    for(auto pixel:t.frame()) for(unsigned shift:{0u,8u,16u,24u}) out.put(char(pixel>>shift));
    expect(bool(out),"Cannot write frame");
}
void store_checks() {
    Temp temp;alenka::Profile first;
    {
        alenka::ProfileStore store(temp.path,{200,163});std::string warning;
        first=store.startup(warning);expect(first.name==u8"Игрок"&&warning.empty(),"First startup failed");
        rejects([&]{alenka::ProfileStore second(temp.path);},"Second writer acquired lock");
        first.name=u8"Ёжик_Test-1";first.courses[0].line=42;
        first.courses[0].counts[139]={5000000000ull,70000,2};
        first.courses[0].total_count=70002;first.courses[0].total_ticks=5000000000ull;
        first.settings={0x43,1,400,false};first.language=alenka::CourseLanguage::English;
        store.save(first);store.select(first.id);
        auto second=first;second.id=alenka::new_profile_id();second.name=u8"Другой";store.save(second);
        expect(store.startup(warning).id==first.id,"File date replaced explicit last profile");
        auto p=store.startup(warning);
        expect(p.courses[0].counts[139].ticks==5000000000ull&&p.courses[0].counts[139].correct==70000&&
            !p.settings.sound&&p.courses[0].line==42,"64-bit statistics or settings lost");
        auto path=temp.path/"profiles"/(first.id+".ini");auto good=read(path);
        for(const auto& mutation:std::vector<std::pair<std::string,std::string>>{
                {"version=1","version=99"},{"rhythm=400","rhythm=0"},{"line=42","line=999"},
                {"70000,2","18446744073709551616,2"},{"k255=0,0,0\n",""}}) {
            auto bad=good;auto pos=bad.find(mutation.first);expect(pos!=bad.npos,"Missing corruption target");
            bad.replace(pos,mutation.first.size(),mutation.second);write(path,bad);warning.clear();
            expect(store.list(warning).size()==1&&!warning.empty(),"Corrupt profile accepted");
            rejects([&]{store.save(first);},"Corrupt profile overwritten");expect(read(path)==bad,"Corrupt file changed");
        }
        write(path,good);store.select(alenka::new_profile_id());warning.clear();
        store.startup(warning);expect(!warning.empty(),"Missing last profile was silent");store.select(first.id);
    }
    { alenka::ProfileStore store(temp.path);std::string warning;expect(store.startup(warning).id==first.id,"Lock or last profile lost"); }
    expect(alenka::normalize_name(u8"  Анна  ")==u8"Анна","Trimming failed");
    expect(alenka::name_key(u8"ЁЖИК")==alenka::name_key(u8"ёжик"),"Russian case folding failed");
    for(const auto& name:{std::string("../escape"),std::string(33,'a'),std::string("   "),std::string("\xc0\xaf")})
        rejects([&]{alenka::normalize_name(name);},"Invalid name accepted");
    std::cout<<"Storage, validation, atomic replacement, UTF-8 and locking passed\n";
}
void session_checks(const alenka::Original& original) {
    {
        Temp completed;Trainer t(original,alenka::Screen::main);alenka::Session session(t,completed.path);
        tap(t,28);
        while(t.practicing()) { tap(t,alenka::TrainerFixture::expected(t));session.update(); }
        expect(t.line()==1&&total(t)>0,"Completed line did not advance");
        auto saved=read(completed.path/"profiles"/(t.profile().id+".ini"));
        expect(saved.find("line=1\n")!=saved.npos&&saved.find("total_count="+std::to_string(total(t)))!=saved.npos,
               "Completed line was not saved immediately");
        setting(t,0);tap(t,28);for(unsigned i=0;i<5;++i) tap(t,14);
        t.text_input(u8"Новое имя");tap(t,28);session.update();
        expect(t.profile().name==u8"Новое имя"&&total(t)>0,"Rename lost progress");
        expect(fs::exists(completed.path/"profiles"/(t.profile().id+".ini")),"Rename changed identity");
    }
    Temp temp;std::string first_id,second_id;
    {
        Trainer t(original,alenka::Screen::main);alenka::Session session(t,temp.path);first_id=t.profile().id;
        tap(t,28);tap(t,37);expect(t.position()==1&&total(t)==1,"Partial attempt failed");
        tap(t,1);expect(t.menu(),"Esc did not return to menu");session.update();
        alenka::TrainerFixture::select(t,7);tap(t,96);session.update();expect(t.line()==1,"Manual line selection failed");
        setting(t,3);session.update();auto shared_flags=t.flags();
        setting(t,1);session.update();expect(t.line()==0&&total(t)==1,"English switch erased Russian progress");
        expect(t.flags()==shared_flags,"Course switch changed common settings");
        setting(t,2);session.update();expect(!t.sound_enabled(),"Mute failed");
        setting(t,1);session.update();expect(t.line()==1&&total(t)==1&&!t.sound_enabled(),"Russian progress or common settings lost");
        setting(t,0);tap(t,96);tap(t,28);session.update();
        tap(t,95);tap(t,28);expect(t.editing_name(),"New editor did not open");
        t.text_input(u8"Анна");tap(t,28);session.update();second_id=t.profile().id;
        expect(second_id!=first_id&&t.profile().name==u8"Анна"&&total(t)==0,"New profile isolation failed");
        setting(t,0);tap(t,28);expect(t.editing_name(),"Rename editor did not open");
        for(unsigned i=0;i<4;++i) tap(t,14);
        t.text_input(u8"игрок");tap(t,28);session.update();
        expect(t.editing_name()&&t.profile().name==u8"Анна","Duplicate rename accepted");tap(t,1);tap(t,1);
        fs::rename(temp.path/"profiles",temp.path/"parked");write(temp.path/"profiles","blocked");
        setting(t,2);expect(!session.update(),"Save failure terminated session");
        fs::remove(temp.path/"profiles");fs::rename(temp.path/"parked",temp.path/"profiles");
        tap(t,28);session.update();expect(!t.save_needed(),"Retry failed");
        tap(t,28);tap(t,37);expect(t.position()==1,"Exit partial attempt failed");expect(session.update(true),"Close failed");
    }
    {
        Trainer t(original,alenka::Screen::main);alenka::Session session(t,temp.path);
        expect(t.profile().id==second_id&&t.position()==0&&t.menu()&&total(t)==1,"Partial restart failed");
        setting(t,0);tap(t,96);tap(t,28);session.update();tap(t,96);tap(t,28);
        fs::rename(temp.path/"profiles",temp.path/"parked");write(temp.path/"profiles","blocked");
        session.update();expect(t.profile().id==second_id,"Profile switched before current save succeeded");
        fs::remove(temp.path/"profiles");fs::rename(temp.path/"parked",temp.path/"profiles");
        tap(t,28);session.update();
        expect(t.profile().id==first_id&&t.line()==1&&total(t)==1&&!t.sound_enabled(),"Profile switch failed");
        tap(t,1);tap(t,99);tap(t,57);session.update();expect(total(t)==0,"Reset failed");tap(t,1);
        auto path=temp.path/"profiles"/(first_id+".ini");write(path,"broken");
        expect(!session.update(true),"Corrupt active profile overwritten");
        tap(t,96);tap(t,28);expect(!session.update(),"Cancel close terminated session");
        expect(!session.update(true),"Close accepted corrupt file");
        tap(t,96);tap(t,96);tap(t,28);expect(session.update(),"Explicit discard did not close");
        expect(read(path)=="broken","Discard changed corrupt data");
    }
    std::cout<<"Session checkpoints, switching, duplicate names, retry/cancel/discard and partial restart passed\n";
}
}
int main(int argc,char** argv) {
    try {
        expect(argc==2||argc==3,"Usage: profile-tests DATA [CAPTURE-DIRECTORY]");
        alenka::Original original(fs::u8path(argv[1]));store_checks();session_checks(original);
        for(unsigned confirm:{28u,100u}) {
            Trainer start(original);tap(start,28);auto help=start.frame();
            expect(!start.practicing(),"Opening startup help started an exercise");
            tap(start,confirm);
            expect(start.frame()!=help&&start.menu()&&!start.practicing(),"Enter did not leave startup help for the menu");
        }
        {
            Trainer glyph(original,alenka::Screen::main);auto background=glyph.frame();
            auto actual=alenka::TrainerFixture::letter(glyph);
            const char* ink[]={"..###...",".#..#...","#...#...","#...#...","#####...","#...#...","........","........"};
            for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
                unsigned i=(80+y)*640+80+x;
                expect(actual[i]==(ink[y][x]=='#'?0xffffff00u:background[i]),"Regular font includes shadow or deforms original ink");
            }
            tap(glyph,59);actual=glyph.frame();auto reference=original.render(alenka::Screen::help);
            for(unsigned i=0;i<actual.size();++i) {
                unsigned x=i%640,y=i/640;
                if(!((x>=64&&x<624&&y>=272&&y<296)||(x>=8&&x<632&&y>=328&&y<344)))
                    expect(actual[i]==reference[i],"Original help artwork or gray background changed");
            }
            tap(glyph,99);expect(glyph.frame()!=actual,"Extra profile help did not open");
            tap(glyph,92);expect(glyph.frame()==actual,"Original help page did not restore");
            tap(glyph,1);tap(glyph,1);expect(alenka::TrainerFixture::original_buttons(glyph),"Statistics artwork was replaced");
            tap(glyph,1);setting(glyph,0);tap(glyph,28);actual=glyph.frame();
            expect(actual[150*640+470]==reference[300*640+320],"Profile dialog lost the original gray background");
        }
        Trainer t(original,alenka::Screen::main);
        {
            auto named=t.profile();named.name=u8"Ёё";t.load_profile(named);auto pixels=t.frame();
            unsigned ink=0;
            for(unsigned y=40;y<48;++y) for(unsigned x=84;x<100;++x) ink+=pixels[y*640+x]==0xffffffffu;
            expect(ink>8&&ink<100,"Yo letters rendered as solid atlas tiles");
            for(unsigned x:{86u,89u,94u,97u}) expect(pixels[40*640+x]==0xffffffffu,"Yo dots missing");
            t.load_profile(alenka::Profile{});
        }
        {
            auto before=t.frame();auto long_profile=t.profile();long_profile.name.clear();
            for(unsigned i=0;i<32;++i) long_profile.name+=u8"Я";
            t.load_profile(long_profile);auto after=t.frame();
            for(unsigned i=0;i<before.size();++i) {
                unsigned x=i%640,y=i/640;
                if(x<48||x>=136||y<24||y>=56) expect(before[i]==after[i],"Long profile name escaped its field");
            }
            setting(t,0);tap(t,28);t.text_input(u8"Я");tap(t,28);
            expect(t.take_action().value==long_profile.name,"Editor exceeded 32 characters");
            t.load_profile(alenka::Profile{});
            alenka::TrainerFixture::select(t,1);
            t.key(95,true,++clock_ms);t.key(95,true,++clock_ms);
            expect(t.course_language()==alenka::CourseLanguage::English,"Held language key toggled twice");
            t.key(95,false,++clock_ms);
        }
        auto p=t.profile();p.language=alenka::CourseLanguage::Russian;
        p.courses[0].counts[139]={std::numeric_limits<std::uint64_t>::max(),1,std::numeric_limits<std::uint64_t>::max()-1};
        p.courses[0].total_count=p.courses[0].total_ticks=std::numeric_limits<std::uint64_t>::max();
        t.load_profile(p);tap(t,1);t.frame();tap(t,1);
        if(argc==3) {
            fs::path dir=fs::u8path(argv[2]);fs::create_directories(dir);
            Trainer ui(original,alenka::Screen::main);capture(ui,dir/"main.raw");
            setting(ui,1);capture(ui,dir/"english.raw");setting(ui,2);capture(ui,dir/"muted.raw");
            tap(ui,1);capture(ui,dir/"statistics.raw");tap(ui,1);tap(ui,59);capture(ui,dir/"help.raw");
            tap(ui,99);capture(ui,dir/"help-profiles.raw");tap(ui,1);
            setting(ui,0);capture(ui,dir/"profile-menu.raw");tap(ui,28);ui.text_input(u8"_Ёжик");capture(ui,dir/"rename.raw");
            tap(ui,1);std::vector<alenka::Profile> profiles;
            for(unsigned i=0;i<24;++i) { alenka::Profile a;a.id=alenka::new_profile_id();a.name=u8"Длинное имя профиля номер "+std::to_string(i);profiles.push_back(a); }
            ui.show_profiles(profiles);for(unsigned i=0;i<17;++i) tap(ui,96);capture(ui,dir/"profiles.raw");
            ui.show_error(true);capture(ui,dir/"save-error.raw");
        }
        std::cout<<"Profile checks passed\n";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
