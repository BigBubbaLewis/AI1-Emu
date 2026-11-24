// SPDX-License-Identifier: MIT
#pragma once
#include <SDL.h>
#include <algorithm>
struct DualScreenLayout {SDL_Rect top,bottom;};
inline DualScreenLayout dualScreenLayout(int layout){switch(layout%3){case 1:return {{400,0,480,360},{400,360,480,360}};case 2:return {{0,0,960,720},{976,480,288,216}};default:return {{16,126,608,456},{656,126,608,456}};}}
inline bool bottomScreenPoint(const SDL_Rect& rect,float x,float y,float& dsX,float& dsY){if(x<rect.x||y<rect.y||x>=rect.x+rect.w||y>=rect.y+rect.h)return false;dsX=std::clamp((x-rect.x)*256/rect.w,0.f,255.f);dsY=std::clamp((y-rect.y)*192/rect.h,0.f,191.f);return true;}
