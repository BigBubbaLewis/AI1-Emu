// SPDX-License-Identifier: MIT
#pragma once
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
inline uint32_t dsAudioTargetBytes(unsigned rate){return uint32_t((uint64_t(rate)*4*96+999)/1000);}
inline double dsAudioFrameInterval(double fps,uint32_t queued,uint32_t target,bool playing){
 double period=1.0/(fps>1?fps:60);if(!playing||!target)return period;
 if(queued<target/2)return period*.995;if(queued>target+target/2)return period*1.005;return period;
}

// v0.5.5: steady playback speed.
// The game's real speed is estimated from how many samples it produces per second,
// averaged over about two seconds. Sound plays at that steady rate: consistently
// slower when the game runs slow, normal at full speed, instead of speeding up and
// slowing down with every small change in the buffer (which sounded random).
constexpr float dsAudioMaxStretch=1.6f;      // slowest playback: 1/1.6 = 62% speed
constexpr double dsAudioSpeedWindow=2.0;     // seconds of averaging for the speed estimate
constexpr float dsAudioMaxTrim=0.04f;        // at most 4% correction to keep the buffer level

struct DSAudioSpeed {
 double speed=1.0,last=0;bool primed=false;
 // frames produced since the previous call at time `now` (seconds); returns the smoothed speed.
 double update(size_t frames,unsigned rate,double now){
  if(!primed){primed=true;last=now;return speed;}
  double dt=now-last;last=now;
  if(dt<=0||dt>0.5||!rate)return speed; // ignore pauses (menu, loading)
  double instant=std::clamp(double(frames)/rate/dt,0.2,2.0);
  speed+=(instant-speed)*(dt/(dsAudioSpeedWindow+dt));
  return speed;
 }
 void reset(){speed=1.0;primed=false;}
};

// Playback stretch for a given speed and buffer level: 1 = normal speed.
inline float dsAudioTargetStretch(double speed,uint32_t queued,uint32_t target){
 float f=float(1.0/std::max(speed,1.0/double(dsAudioMaxStretch)));
 if(target){float error=(float(target)-float(queued))/float(target);f*=1.f+std::clamp(error,-1.f,1.f)*dsAudioMaxTrim;}
 return std::clamp(f,1.f-dsAudioMaxTrim,dsAudioMaxStretch);
}

// Linear resampler that keeps its position between blocks, so block edges do not click.
struct DSAudioStretcher {
 double phase=-1;int16_t prev[2]={0,0};bool primed=false;
 void reset(){phase=-1;primed=false;}
 // Appends the stretched block to `out` (interleaved stereo). factor 1.25 = 25% more output.
 void process(const int16_t* in,size_t frames,float factor,std::vector<int16_t>& out){
  out.clear();if(!frames)return;
  if(!primed){prev[0]=in[0];prev[1]=in[1];primed=true;}
  double step=1.0/std::max(0.5f,factor),pos=phase,last=double(frames-1);
  out.reserve(size_t((frames+2)*std::max(1.f,factor))*2);
  while(pos<last){
   double base=std::floor(pos);long i0=long(base);double t=pos-base;
   for(int c=0;c<2;++c){
    float s0=i0<0?float(prev[c]):float(in[size_t(i0)*2+c]),s1=float(in[size_t(i0+1)*2+c]);
    out.push_back(int16_t(std::lround(s0+(s1-s0)*t)));
   }
   pos+=step;
  }
  phase=pos-double(frames);
  prev[0]=in[(frames-1)*2];prev[1]=in[(frames-1)*2+1];
 }
};

// Startup cushion, then continuous playback: an underrun never pauses the device
// (pausing until ~0.1 s refilled produced audible on/off chopping). No amplitude
// changes or queued-music deletion.
struct DSAudioQueue {
 SDL_AudioDeviceID device=0;unsigned rate=32768,underruns=0,waits=0;bool playing=false;
 DSAudioSpeed speed;DSAudioStretcher stretcher;float factor=1.f;
 std::vector<int16_t> stretched;
 void configure(SDL_AudioDeviceID id,unsigned hz){device=id;rate=hz;playing=false;underruns=waits=0;factor=1.f;speed.reset();stretcher.reset();if(device)SDL_PauseAudioDevice(device,1);}
 void reset(){if(device){SDL_PauseAudioDevice(device,1);SDL_ClearQueuedAudio(device);}playing=false;factor=1.f;speed.reset();stretcher.reset();}
 // `now` is the time in seconds; leave it negative to use the system clock.
 bool push(const int16_t* data,size_t frames,bool(*abort)()=nullptr,double now=-1){
  if(!device||!frames)return true;
  if(now<0)now=double(SDL_GetPerformanceCounter())/double(SDL_GetPerformanceFrequency());
  auto queued=SDL_GetQueuedAudioSize(device);auto target=dsAudioTargetBytes(rate);
  if(playing&&!queued)++underruns;
  if(queued>target*2){++waits;while(SDL_GetQueuedAudioSize(device)>target*2){if(abort&&abort())return false;SDL_Delay(1);}queued=SDL_GetQueuedAudioSize(device);}
  double estimate=speed.update(frames,rate,now);
  // Before playback starts the stretch stays at 1; afterwards it glides (about 0.25 s)
  // toward the steady target, so the pitch never jumps.
  float want=playing?dsAudioTargetStretch(estimate,queued,target):1.f;
  factor+=(want-factor)*0.08f;
  stretcher.process(data,frames,factor,stretched);
  if(!stretched.empty()&&SDL_QueueAudio(device,stretched.data(),Uint32(stretched.size()*sizeof(int16_t)))!=0)return false;
  if(!playing&&SDL_GetQueuedAudioSize(device)>=target){SDL_PauseAudioDevice(device,0);playing=true;}return true;
 }
};
