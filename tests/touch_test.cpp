#define SDL_MAIN_HANDLED
#include "library_ui.h"
#include <cassert>
#include <iostream>
#include <cmath>
int main(){
 SDL_Init(SDL_INIT_VIDEO);{LibraryUI ui(nullptr);size_t selected=0;bool cartridge=false;
 auto tap=[&](int x,int y){SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;ui.handlePointer(e,9,selected,cartridge);e.type=SDL_MOUSEBUTTONUP;return ui.handlePointer(e,9,selected,cartridge);};
 // v0.4 tab strip: Games, Map, Controls, Settings, About (134 px each from x=104).
 tap(573,120);assert(ui.tab==LibraryUI::TabSettings);assert(tap(900,300)==LibraryUI::ChangeView);assert(tap(900,410)==LibraryUI::Sort);
 assert(tap(620,687)==LibraryUI::ChangeView);assert(tap(750,687)==LibraryUI::Sort);assert(tap(900,687)==LibraryUI::Update);assert(tap(1010,687)==LibraryUI::Back);assert(tap(1150,687)==LibraryUI::None&&ui.tab==LibraryUI::TabSettings);
 tap(707,120);assert(ui.tab==LibraryUI::TabAbout);assert(tap(620,687)==LibraryUI::None);tap(1150,687);assert(ui.tab==LibraryUI::TabSettings);
 tap(305,120);assert(ui.tab==LibraryUI::TabMap);assert(tap(620,687)==LibraryUI::Previous);assert(tap(750,687)==LibraryUI::Next);assert(tap(1010,687)==LibraryUI::Back);
 tap(439,120);assert(ui.tab==LibraryUI::TabControls);assert(tap(620,687)==LibraryUI::None);assert(tap(1150,687)==LibraryUI::None&&ui.tab==LibraryUI::TabControls);assert(tap(1010,687)==LibraryUI::Back);
 tap(1160,120);assert(!cartridge); // the view toggle only exists on the Games tab
 tap(171,120);assert(ui.tab==LibraryUI::TabGames);
 {int order[5];for(int i=0;i<5;++i){order[i]=ui.tab;ui.cycleTab(1);}assert(order[0]==LibraryUI::TabGames&&order[1]==LibraryUI::TabMap&&order[2]==LibraryUI::TabControls&&order[3]==LibraryUI::TabSettings&&order[4]==LibraryUI::TabAbout&&ui.tab==LibraryUI::TabGames);ui.cycleTab(-1);assert(ui.tab==LibraryUI::TabAbout);ui.tab=LibraryUI::TabGames;}
 tap(1160,120);assert(cartridge);tap(1000,120);assert(!cartridge);
 tap(970,168);assert(ui.category==9&&ui.categoryChanged);ui.categoryChanged=false;tap(1150,168);assert(ui.category==11&&ui.categoryChanged);ui.categoryChanged=false;tap(130,168);assert(ui.category==0&&ui.categoryChanged);
 assert(tap(350,220)==LibraryUI::None);assert(selected==1);assert(tap(350,220)==LibraryUI::Play);
 assert(tap(620,687)==LibraryUI::Play);assert(tap(750,687)==LibraryUI::ChangeView);assert(tap(900,687)==LibraryUI::Sort);assert(tap(1010,687)==LibraryUI::Back);tap(1150,687);assert(ui.tab==1);ui.tab=0;
 SDL_Event e{};e.type=SDL_FINGERDOWN;e.tfinger.fingerId=7;e.tfinger.x=.7f;e.tfinger.y=.4f;ui.handlePointer(e,9,selected,cartridge);e.type=SDL_FINGERUP;e.tfinger.x=.3f;assert(ui.handlePointer(e,9,selected,cartridge)==LibraryUI::None);assert(ui.scroll>0);for(int i=0;i<120;i++)ui.advanceScroll(9,selected,1.f/60);assert(ui.scroll>=0&&ui.scroll<=900);assert(!ui.freeScroll);
 e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.which=SDL_TOUCH_MOUSEID;e.button.button=SDL_BUTTON_LEFT;e.button.x=320;e.button.y=120;ui.handlePointer(e,9,selected,cartridge);e.type=SDL_MOUSEBUTTONUP;assert(ui.handlePointer(e,9,selected,cartridge)==LibraryUI::None);assert(ui.tab==0);
 assert(tap(60,600)==LibraryUI::None);
 }
 {LibraryUI a(nullptr),b(nullptr);size_t sa=2,sb=2;a.scroll=b.scroll=100;a.freeScroll=b.freeScroll=true;a.scrollVelocity=b.scrollVelocity=1000;a.scrollSelection=b.scrollSelection=2;
 for(int i=0;i<60;i++)a.advanceScroll(20,sa,1.f/60);for(int i=0;i<120;i++)b.advanceScroll(20,sb,1.f/120);assert(std::abs(a.scroll-b.scroll)<2);
 a.scroll=0;a.freeScroll=false;sa=0;bool cartridge=false;SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=800;e.button.y=250;e.common.timestamp=100;a.handlePointer(e,20,sa,cartridge);
 e={};e.type=SDL_MOUSEMOTION;e.motion.x=600;e.motion.y=250;e.common.timestamp=116;a.handlePointer(e,20,sa,cartridge);assert(a.dragging&&a.scroll==200);a.advanceScroll(20,sa,.03f);assert(a.scroll==200);
 e={};e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;e.button.x=500;e.button.y=250;e.common.timestamp=132;assert(a.handlePointer(e,20,sa,cartridge)==LibraryUI::None);assert(a.scroll==300);a.advanceScroll(20,sa,.016f);assert(a.scroll>300);
 for(int i=0;i<300;i++)a.advanceScroll(20,sa,1.f/60);assert(!a.freeScroll);assert(std::abs(a.scroll/225-std::round(a.scroll/225))<.001f);
 a.advanceScroll(0,sa,.016f);assert(a.scroll>=0);
 }
 SDL_Quit();std::cout<<"PASS: five tabs, tab cycling, per-tab footer actions, view, selection, launch, settings, swipe, synthetic-mouse suppression and empty hit.\n";
}



