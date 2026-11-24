#pragma once
#include <SDL.h>
#include <cctype>
// Original 5x7 glyphs, padded to the previous eight-pixel text advance.
inline void pixelText(SDL_Renderer* r,int x,int y,const char* text,
                      Uint8 red,Uint8 green,Uint8 blue,Uint8 alpha) {
 static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ./:-_+?";
 static const Uint8 glyphs[][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,0,0,0,0,0,0},{0,0,0,0,0,12,12},
 {1,2,2,4,8,8,16},{0,4,4,0,4,4,0},{0,0,0,31,0,0,0},
 {0,0,0,0,0,0,31},{0,4,4,31,4,4,0},{14,17,1,2,4,0,4}};
 SDL_SetRenderDrawColor(r,red,green,blue,alpha);
 for(;text&&*text;++text,x+=8){
   unsigned char c=static_cast<unsigned char>(*text);
   c=static_cast<unsigned char>(std::toupper(c));
   unsigned index=0;while(chars[index]&&chars[index]!=c)++index;
   if(!chars[index])index=sizeof(chars)-2;
   for(int row=0;row<7;++row)for(int col=0;col<5;++col)
     if(glyphs[index][row]&(16>>col)){SDL_Rect p{x+col+1,y+row,1,1};SDL_RenderFillRect(r,&p);}
 }
}
