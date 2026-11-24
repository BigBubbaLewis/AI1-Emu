// SPDX-License-Identifier: MIT
#pragma once
#include <SDL.h>
#include <cstdint>
inline uint32_t dsAudioTargetBytes(unsigned rate){return uint32_t((uint64_t(rate)*4*96+999)/1000);}
inline double dsAudioFrameInterval(double fps,uint32_t queued,uint32_t target,bool playing){
 double period=1.0/(fps>1?fps:60);if(!playing||!target)return period;
 if(queued<target/2)return period*.995;if(queued>target+target/2)return period*1.005;return period;
}
// Small startup/underrun cushion; no amplitude changes or queued-music deletion.
struct DSAudioQueue {
 SDL_AudioDeviceID device=0;unsigned rate=32768,underruns=0,waits=0;bool playing=false;
 void configure(SDL_AudioDeviceID id,unsigned hz){device=id;rate=hz;playing=false;underruns=waits=0;if(device)SDL_PauseAudioDevice(device,1);}
 void reset(){if(device){SDL_PauseAudioDevice(device,1);SDL_ClearQueuedAudio(device);}playing=false;}
 bool push(const int16_t* data,size_t frames,bool(*abort)()=nullptr){
  if(!device||!frames)return true;auto queued=SDL_GetQueuedAudioSize(device);auto target=dsAudioTargetBytes(rate);
  if(playing&&!queued){SDL_PauseAudioDevice(device,1);playing=false;++underruns;}
  if(queued>target*2){++waits;while(SDL_GetQueuedAudioSize(device)>target*2){if(abort&&abort())return false;SDL_Delay(1);}}
  if(SDL_QueueAudio(device,data,frames*4)!=0)return false;
  if(!playing&&SDL_GetQueuedAudioSize(device)>=target){SDL_PauseAudioDevice(device,0);playing=true;}return true;
 }
};
