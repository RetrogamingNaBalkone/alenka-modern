#include "profile.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace alenka {
namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
bool valid_id(const std::string& id) {
    return id.size()==32&&id.find_first_not_of("0123456789abcdef")==std::string::npos;
}
std::vector<unsigned> decode(const std::string& s) {
    std::vector<unsigned> result;
    for(std::size_t i=0;i<s.size();) {
        unsigned c=static_cast<unsigned char>(s[i++]);
        if(c>=0x80) {
            require(c>=0xc2&&c<=0xdf&&i<s.size(),"Invalid name encoding");
            unsigned next=static_cast<unsigned char>(s[i++]);
            require((next&0xc0)==0x80,"Invalid name encoding");
            c=((c&31)<<6)|(next&63);
        }
        result.push_back(c);
    }
    return result;
}
void atomic_write(const std::filesystem::path& path,const std::string& contents) {
    auto temp=path;temp+=std::string(".")+new_profile_id()+".tmp";
    try {
#ifdef _WIN32
        HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(file!=INVALID_HANDLE_VALUE,"Cannot create save file");
        DWORD written=0;
        bool ok=WriteFile(file,contents.data(),DWORD(contents.size()),&written,nullptr)&&written==contents.size();
        ok=ok&&FlushFileBuffers(file);CloseHandle(file);
        require(ok,"Cannot flush save file");
        require(MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,
                "Cannot replace save file");
#else
        int file=::open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
        require(file>=0,"Cannot create save file");
        std::size_t done=0;
        while(done<contents.size()) {
            auto n=::write(file,contents.data()+done,contents.size()-done);
            if(n<=0) { ::close(file);throw std::runtime_error("Cannot write save file"); }
            done+=std::size_t(n);
        }
        int flushed=::fsync(file);::close(file);require(flushed==0,"Cannot flush save file");
        std::filesystem::rename(temp,path);
        int dir=::open(path.parent_path().c_str(),O_RDONLY);
        if(dir>=0) { ::fsync(dir);::close(dir); }
#endif
    } catch(...) { std::error_code ec;std::filesystem::remove(temp,ec);throw; }
}
using Ini=std::map<std::string,std::string>;
Ini read_ini(const std::filesystem::path& path) {
    require(std::filesystem::file_size(path)<=131072,"Save file too large");
    std::ifstream in(path,std::ios::binary);require(bool(in),"Cannot read save file");
    Ini result;std::string section,line;
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r') line.pop_back();
        if(line.empty()) continue;
        if(line.front()=='['&&line.back()==']') { section=line.substr(1,line.size()-2);continue; }
        auto eq=line.find('=');require(eq!=std::string::npos&&!section.empty(),"Invalid INI line");
        require(result.emplace(section+"."+line.substr(0,eq),line.substr(eq+1)).second,"Duplicate INI key");
    }
    require(in.eof(),"Cannot read save file");return result;
}
std::string value(const Ini& ini,const std::string& key) {
    auto it=ini.find(key);require(it!=ini.end(),"Incomplete save file");return it->second;
}
std::uint64_t number(const std::string& s) {
    std::uint64_t n=0;auto parsed=std::from_chars(s.data(),s.data()+s.size(),n);
    require(!s.empty()&&parsed.ec==std::errc{}&&parsed.ptr==s.data()+s.size(),"Invalid save number");return n;
}
Profile read_profile(const std::filesystem::path& path) {
    auto ini=read_ini(path);Profile p;
    require(value(ini,"profile.version")=="1","Unsupported profile version");
    p.id=value(ini,"profile.id");require(valid_id(p.id)&&path.stem()==p.id,"Invalid profile ID");
    p.name=value(ini,"profile.name");require(normalize_name(p.name)==p.name,"Invalid profile name");
    auto language=value(ini,"profile.language");require(language=="ru"||language=="en","Invalid course");
    p.language=language=="ru"?CourseLanguage::Russian:CourseLanguage::English;
    auto flags=number(value(ini,"settings.flags")),hint=number(value(ini,"settings.hint"));
    auto rhythm=number(value(ini,"settings.rhythm")),sound=number(value(ini,"settings.sound"));
    const unsigned hint_flags[]={6,3,2};
    require(hint<=2&&flags<=255&&(flags&~0x5full)==0&&(flags&15)==hint_flags[hint]&&
            rhythm>=10&&rhythm<=400&&rhythm%10==0&&sound<=1,"Invalid profile settings");
    p.settings={unsigned(flags),unsigned(hint),unsigned(rhythm),sound!=0};
    for(unsigned lang=0;lang<2;++lang) {
        auto prefix=std::string(lang?"en.":"ru.");auto& course=p.courses[lang];
        auto line=number(value(ini,prefix+"line"));require(line<=999,"Invalid course line");course.line=unsigned(line);
        course.total_count=number(value(ini,prefix+"total_count"));course.total_ticks=number(value(ini,prefix+"total_ticks"));
        std::uint64_t sum=0,ticks=0;
        for(unsigned c=0;c<256;++c) {
            auto s=value(ini,prefix+"k"+std::to_string(c));auto a=s.find(','),b=s.find(',',a==s.npos?0:a+1);
            require(a!=s.npos&&b!=s.npos,"Invalid character statistics");
            auto& count=course.counts[c];count={number(s.substr(0,a)),number(s.substr(a+1,b-a-1)),number(s.substr(b+1))};
            for(auto n:{count.correct,count.errors}) {
                require(n<=std::numeric_limits<std::uint64_t>::max()-sum,"Statistics overflow");sum+=n;
            }
            require(count.ticks<=std::numeric_limits<std::uint64_t>::max()-ticks,"Statistics overflow");ticks+=count.ticks;
        }
        require(sum==course.total_count&&ticks<=course.total_ticks,"Inconsistent statistics");
    }
    return p;
}
}
std::string name_glyphs(const std::string& utf8) {
    std::string result;
    for(auto c:decode(utf8)) {
        unsigned glyph=c;
        if(c>=0x410&&c<=0x43f) glyph=c-0x410+0x80;
        else if(c>=0x440&&c<=0x44f) glyph=c-0x440+0xe0;
        else if(c==0x401) glyph=0xf0;
        else if(c==0x451) glyph=0xf1;
        else require((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='-'||c=='_',
                     "Unsupported name character");
        result.push_back(char(glyph));
    }
    return result;
}
std::string normalize_name(const std::string& utf8) {
    auto begin=utf8.find_first_not_of(' '),end=utf8.find_last_not_of(' ');
    require(begin!=std::string::npos,"Empty profile name");auto result=utf8.substr(begin,end-begin+1);
    require(name_glyphs(result).size()<=32,"Profile name exceeds 32 characters");return result;
}
std::string name_key(const std::string& utf8) {
    auto s=name_glyphs(utf8);
    for(auto& raw:s) {
        auto c=static_cast<unsigned char>(raw);
        if(c>='A'&&c<='Z') raw=char(c+32);
        else if(c>=0x80&&c<=0x8f) raw=char(c+32);
        else if(c>=0x90&&c<=0x9f) raw=char(c+80);
        else if(c==0xf0) raw=char(0xf1);
    }
    return s;
}
std::string new_profile_id() {
    std::random_device random;std::string result;
    for(unsigned i=0;i<32;++i) result+="0123456789abcdef"[random()&15];
    return result;
}
ProfileStore::ProfileStore(const std::filesystem::path& root,std::array<unsigned,2> lengths):root_(root),course_lengths_(lengths) {
    auto path=root_/".alenka.lock";
#ifdef _WIN32
    auto handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(handle!=INVALID_HANDLE_VALUE,"Data directory is locked or not writable");lock_=reinterpret_cast<std::intptr_t>(handle);
#else
    int fd=::open(path.c_str(),O_CREAT|O_RDWR,0600);require(fd>=0,"Cannot lock data directory");
    if(::flock(fd,LOCK_EX|LOCK_NB)) { ::close(fd);throw std::runtime_error("Data directory is locked"); }lock_=fd;
#endif
    try { std::filesystem::create_directories(root_/"profiles"); }
    catch(...) {
#ifdef _WIN32
        CloseHandle(reinterpret_cast<HANDLE>(lock_));
#else
        ::close(int(lock_));
#endif
        throw;
    }
}
ProfileStore::~ProfileStore() {
#ifdef _WIN32
    if(lock_!=-1) CloseHandle(reinterpret_cast<HANDLE>(lock_));
#else
    if(lock_!=-1) ::close(int(lock_));
#endif
}
std::vector<Profile> ProfileStore::list(std::string& warning) const {
    std::vector<Profile> result;
    for(const auto& entry:std::filesystem::directory_iterator(root_/"profiles")) {
        if(entry.path().extension()!=".ini") continue;
        try {
            auto p=read_profile(entry.path());
            require(p.courses[0].line<course_lengths_[0]&&p.courses[1].line<course_lengths_[1],"Invalid course line");
            require(std::none_of(result.begin(),result.end(),[&](const Profile& other){return name_key(other.name)==name_key(p.name);}),
                    "Duplicate profile name");
            result.push_back(std::move(p));
        } catch(const std::exception&) { warning=u8"Повреждённые профили пропущены"; }
    }
    std::sort(result.begin(),result.end(),[](const Profile& a,const Profile& b){return name_key(a.name)<name_key(b.name);});return result;
}
Profile ProfileStore::startup(std::string& warning) {
    auto profiles=list(warning);std::string last;
    if(std::filesystem::exists(root_/"alenka.ini")) {
        try {
            auto ini=read_ini(root_/"alenka.ini");require(value(ini,"application.version")=="1","Unsupported settings version");
            last=value(ini,"application.last_profile");require(valid_id(last),"Invalid last profile");
        } catch(const std::exception&) { warning=u8"Настройки запуска повреждены"; }
    }
    if(!last.empty()) {
        auto found=std::find_if(profiles.begin(),profiles.end(),[&](const Profile& p){return p.id==last;});
        if(found!=profiles.end()) return *found;
        warning=u8"Последний профиль недоступен";
    }
    if(!profiles.empty()) return profiles.front();
    Profile p;p.id=new_profile_id();save(p);return p;
}
void ProfileStore::save(const Profile& p) {
    require(valid_id(p.id)&&normalize_name(p.name)==p.name,"Invalid profile");
    require(p.courses[0].line<course_lengths_[0]&&p.courses[1].line<course_lengths_[1],"Invalid course line");
    const unsigned hint_flags[]={6,3,2};
    require(p.settings.hint<3&&(p.settings.flags&~0x5fu)==0&&(p.settings.flags&15)==hint_flags[p.settings.hint]&&
            p.settings.rhythm>=10&&p.settings.rhythm<=400&&p.settings.rhythm%10==0,"Invalid settings");
    for(const auto& course:p.courses) {
        std::uint64_t sum=0,ticks=0;
        for(const auto& c:course.counts) {
            for(auto n:{c.correct,c.errors}) {
                require(n<=std::numeric_limits<std::uint64_t>::max()-sum,"Statistics overflow");sum+=n;
            }
            require(c.ticks<=std::numeric_limits<std::uint64_t>::max()-ticks,"Statistics overflow");ticks+=c.ticks;
        }
        require(sum==course.total_count&&ticks<=course.total_ticks,"Inconsistent statistics");
    }
    // Never overwrite a file that has become corrupt since it was loaded.
    auto path=root_/"profiles"/(p.id+".ini");
    if(std::filesystem::exists(path)) {
        auto old=read_profile(path);
        require(old.courses[0].line<course_lengths_[0]&&old.courses[1].line<course_lengths_[1],"Invalid stored course line");
    }
    std::ostringstream out;
    out<<"[profile]\nversion=1\nid="<<p.id<<"\nname="<<p.name<<"\nlanguage="
       <<(p.language==CourseLanguage::Russian?"ru":"en")<<"\n[settings]\nflags="<<p.settings.flags
       <<"\nhint="<<p.settings.hint<<"\nrhythm="<<p.settings.rhythm<<"\nsound="<<p.settings.sound<<'\n';
    for(unsigned lang=0;lang<2;++lang) {
        const auto& c=p.courses[lang];out<<'['<<(lang?"en":"ru")<<"]\nline="<<c.line
            <<"\ntotal_count="<<c.total_count<<"\ntotal_ticks="<<c.total_ticks<<'\n';
        for(unsigned i=0;i<256;++i) out<<'k'<<i<<'='<<c.counts[i].ticks<<','<<c.counts[i].correct<<','<<c.counts[i].errors<<'\n';
    }
    atomic_write(path,out.str());
}
void ProfileStore::select(const std::string& id) {
    require(valid_id(id),"Invalid profile ID");
    atomic_write(root_/"alenka.ini","[application]\nversion=1\nlast_profile="+id+"\n");
}
}
