#include "../src/Engine.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    quiet::Engine e; int keys=0;
    auto send=[&]{++keys;return true;};
    e.begin(0,60);e.tick(59.9f,true,true,send);assert(keys==0);
    e.tick(60,true,true,send);assert(e.owned && keys==1);
    for(int i=0;i<100;++i)e.tick(70,true,true,send);
    assert(keys==1); // no repeat toggles on every frame
    e.finish(send);e.finish(send);assert(!e.owned && keys==2);
    e.tick(80,true,true,send);assert(keys==2); // dead run stays inactive
    e.begin(0,60);e.tick(61,true,true,send);
    e.tick(61,true,false,send);assert(!e.owned && keys==4); // pause
    e.tick(61,true,true,send);assert(e.owned && keys==5); // resume
    e.tick(61,false,true,send);assert(!e.owned && keys==6); // disable
    e.begin(65,60);e.tick(80,true,true,send);assert(keys==6); // startpos
    e.begin(0,60);e.tick(std::numeric_limits<float>::quiet_NaN(),true,true,send);assert(keys==6);
    e.tick(60,true,true,[]{return false;});assert(!e.owned); // failed input
    e.tick(60,true,true,send);assert(e.owned && keys==7);
    e.finish([]{return false;});assert(e.owned && e.ended); // keep restore pending
    e.finish(send);assert(!e.owned && keys==8);
    e.begin(0,60.5f);e.tick(60.4f,true,true,send);assert(keys==8);
    e.tick(60.5f,true,true,send);assert(keys==9);
    e.finish(send);assert(keys==10);
    std::cout << "PASS: threshold, once per run, death, pause/resume, disable, startpos, NaN, failed input and restore, decimals\n";
}
