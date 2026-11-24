#include "library_ui.h"
#include "platform_library.h"
#include <cmath>
#include <algorithm>
LibraryUI::Action LibraryUI::handlePointer(const SDL_Event& e,size_t count,size_t& selected,bool& cartridge){
 float x=0,y=0;bool press=false,release=false,motion=false;
 if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERUP||e.type==SDL_FINGERMOTION){
  if(pointerDown&&e.tfinger.fingerId!=activeFinger)return None;
  x=e.tfinger.x*1280.f;y=e.tfinger.y*720.f;press=e.type==SDL_FINGERDOWN;release=e.type==SDL_FINGERUP;motion=e.type==SDL_FINGERMOTION;activeFinger=e.tfinger.fingerId;
 }else if(e.type==SDL_MOUSEMOTION){if(e.motion.which==SDL_TOUCH_MOUSEID)return None;x=float(e.motion.x);y=float(e.motion.y);motion=true;
 }else if(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP){
  if(e.button.which==SDL_TOUCH_MOUSEID||e.button.button!=SDL_BUTTON_LEFT)return None;
  x=float(e.button.x);y=float(e.button.y);press=e.type==SDL_MOUSEBUTTONDOWN;release=e.type==SDL_MOUSEBUTTONUP;
 }else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){pointerDown=dragging=dragCandidate=freeScroll=false;scrollVelocity=0;return None;}else return None;
 Uint32 now=e.common.timestamp?e.common.timestamp:SDL_GetTicks();
 if(press){tapBlocked=freeScroll;pointerDown=true;pointerX=lastPointerX=x;pointerY=y;pointerTime=now;dragStartScroll=scroll;scrollVelocity=0;dragging=false;dragCandidate=tab==0&&x>=62&&x<=1218&&y>=168&&y<=413;freeScroll=dragCandidate;scrollSelection=selected;return None;}
 if(pointerDown&&dragCandidate&&(motion||release)){
  float dx=x-pointerX,dy=y-pointerY;
  if(!dragging&&std::fabs(dy)>25&&std::fabs(dy)>std::fabs(dx)){dragCandidate=false;freeScroll=false;tapBlocked=true;}
  else if(dragging||(std::fabs(dx)>10&&std::fabs(dx)>std::fabs(dy))){
   dragging=true;float maximum=std::max(0,int(count)-5)*225.f;scroll=std::clamp(dragStartScroll-dx,0.f,maximum);
   Uint32 elapsed=now-pointerTime;if(elapsed>0&&elapsed<100){float instant=std::clamp((lastPointerX-x)*1000.f/elapsed,-2500.f,2500.f);scrollVelocity=scrollVelocity*.45f+instant*.55f;}else if(elapsed>=100)scrollVelocity=0;
   if(count>5)selected=std::min(count-1,size_t(std::round(scroll/225.f)+2));scrollSelection=selected;
  }
  lastPointerX=x;pointerTime=now;
 }
 if(motion)return None;
 if(!release||!pointerDown)return None;
 pointerDown=false;float dx=x-pointerX,dy=y-pointerY;
 if(dragging){dragging=dragCandidate=false;if(count<=5){freeScroll=false;scrollVelocity=0;return dx<0?Next:Previous;}return None;}
 dragCandidate=freeScroll=false;
 if(std::fabs(dx)>25||std::fabs(dy)>25)return None;
 if(tab==0&&y>=156&&y<=183&&x>=86&&x<1197){int next=int((x-86)*(platforms().size()+1)/1111);if(next!=category){category=next;categoryChanged=true;freeScroll=false;scrollVelocity=0;}return None;}
 if(y>=92&&y<=150){
  if(x>=104&&x<104+tabCount*134){tab=tabOrder[int((x-104)/134)];return None;}
  if(tab==TabGames&&x>=953&&x<=1237){cartridge=x>=1095;return None;}
 }
 if(y>=650){
  // Footer hints change per tab; each slot maps to the action it shows.
  int slot=x>=1092?4:x>=985?3:x>=877?2:x>=704?1:x>=600?0:-1;
  if(slot<0)return None;
  if(slot==4&&tab!=TabSettings&&tab!=TabControls){tab=TabSettings;return None;}
  static const Action games[5]={Play,ChangeView,Sort,Back,None},settings[5]={ChangeView,Sort,Update,Back,None},map[5]={Previous,Next,None,Back,None};
  if(tab==TabGames)return games[slot];if(tab==TabSettings)return settings[slot];if(tab==TabMap)return map[slot];
  return slot==3?Back:None;
 }
 if(tab==1){if(y>=280&&y<=365)return ChangeView;if(y>=385&&y<=470)return Sort;if(x>=85&&x<=1195&&y>=495&&y<=585)return Update;return None;}
 if(tab!=0)return None;
 if(y>=240&&y<=350){if(x<62)return Previous;if(x>1218)return Next;}
 int top=cartridge?222:188,bottom=cartridge?376:402;
 if(x>=62&&x<=1218&&y>=top&&y<=bottom){
  int index=int(std::floor((x-86+int(scroll))/225.f));
  if(index>=0&&size_t(index)<count){float left=86+index*225-int(scroll);if(x>=left&&x<=left+202){if(selected==size_t(index)&&!tapBlocked)return Play;selected=size_t(index);}}
 }
 return None;
}

void LibraryUI::advanceScroll(size_t count,size_t& selected,float dt){
 dt=std::clamp(dt,0.f,.05f);float maximum=std::max(0,int(count)-5)*225.f;
 if(freeScroll&&!pointerDown&&selected!=scrollSelection){freeScroll=false;scrollVelocity=0;}
 if(pointerDown&&dragCandidate)return;
 if(freeScroll){
  if(std::fabs(scrollVelocity)>30){
   float decay=std::exp(-7.f*dt);scroll+=scrollVelocity*(1-decay)/7.f;scrollVelocity*=decay;
   if(scroll<0||scroll>maximum){scroll=std::clamp(scroll,0.f,maximum);scrollVelocity=0;}
  }else{
   scrollVelocity=0;float target=std::clamp(std::round(scroll/225.f)*225.f,0.f,maximum);
   scroll+=(target-scroll)*(1-std::exp(-16.f*dt));if(std::fabs(target-scroll)<.35f){scroll=target;freeScroll=false;}
  }
  if(count>5)selected=std::min(count-1,size_t(std::round(scroll/225.f)+2));scrollSelection=selected;
 }else{
  float target=count?std::max(0,std::min(int(selected)-2,int(count)-5))*225.f:0;
  if(std::fabs(target-scroll)>1350)scroll=target;else scroll+=(target-scroll)*(1-std::exp(-15.f*dt));
 }
}

