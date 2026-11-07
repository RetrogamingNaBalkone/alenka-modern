#include "trainer.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace alenka {
struct TrainerFixture {
    static void counter(Trainer& trainer,unsigned c,std::uint32_t ticks,unsigned correct,unsigned errors) {
        trainer.counts_.at(c)={ticks,std::uint16_t(correct),std::uint16_t(errors)};
    }
};
}

int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Usage: trainer-trace DATA SCRIPT");
        alenka::Original original(std::filesystem::u8path(argv[1]));
        alenka::Trainer trainer(original,alenka::Screen::main);
        std::ifstream script(argv[2]);
        if(!script) throw std::runtime_error("Cannot open trace");
        std::string command;
        unsigned checks=0;
        while(script>>command) {
            if(command[0]=='#') { std::string rest;std::getline(script,rest);continue; }
            if(command=="key") {
                unsigned scan,down; std::uint64_t time;
                script>>scan>>down>>time;
                trainer.key(scan,down!=0,time);
            } else if(command=="advance") {
                std::uint64_t time; script>>time;trainer.advance(time);
            } else if(command=="capture") {
                std::string path;script>>path;
                std::ofstream out(path,std::ios::binary);
                const unsigned char header[]={128,2,0,0,94,1,0,0,0,10,0,0,32,0,0};
                out.write(reinterpret_cast<const char*>(header),15);
                for(auto pixel:trainer.frame()) for(unsigned shift:{0u,8u,16u,24u}) out.put(char(pixel>>shift));
                if(!out) throw std::runtime_error("Cannot write capture");
            } else if(command=="state") std::cout<<trainer.diagnostic()<<'\n';
            else if(command=="check"||command=="check_region") {
                int x=0,y=0,w=640,h=350;
                if(command=="check_region") script>>x>>y>>w>>h;
                if(x<0||y<0||w<1||h<1||x+w>640||y+h>350) throw std::runtime_error("Invalid reference region");
                std::string expected;script>>expected;
                std::uint64_t hash=14695981039346656037ull;
                auto frame=trainer.frame();
                for(int row=y;row<y+h;++row) for(int col=x;col<x+w;++col)
                    for(unsigned shift:{0u,8u,16u,24u}) {
                        hash^=(frame[row*640+col]>>shift)&255;hash*=1099511628211ull;
                    }
                if(hash!=std::stoull(expected,nullptr,16))
                    throw std::runtime_error("DOS reference mismatch at check "+std::to_string(checks+1));
                ++checks;
            }
            else if(command=="counter") {
                unsigned c,ticks,correct,errors;script>>c>>ticks>>correct>>errors;
                alenka::TrainerFixture::counter(trainer,c,ticks,correct,errors);
            }
            else throw std::runtime_error("Unknown trace command: "+command);
            if(!script) throw std::runtime_error("Malformed trace");
        }
        if(checks) std::cout<<checks<<" DOS reference frames passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
