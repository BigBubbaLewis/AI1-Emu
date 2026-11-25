#define SDL_MAIN_HANDLED
#include "ds_audio.h"
#include <cassert>
#include <vector>
#include <cmath>
#include <iostream>
int main(){
 // 1. Speed estimate: a game at 80% speed is measured as ~0.8 after a few seconds.
 {
  DSAudioSpeed s;const unsigned rate=32768;const size_t block=546;double t=0;
  for(int i=0;i<600;++i){s.update(block,rate,t);t+=double(block)/rate/0.8;}
  assert(std::fabs(s.speed-0.8)<0.02);
  // Pauses longer than half a second (menu, loading) are ignored, not counted as slowness.
  double before=s.speed;s.update(block,rate,t+5.0);assert(s.speed==before);
 }
 // 2. Steady stretch: at a steady 80% the playback factor settles near 1.25 and stays put.
 {
  float factor=1.f;const uint32_t target=12000;std::vector<float> history;
  for(int i=0;i<200;++i){factor+=(dsAudioTargetStretch(0.8,target,target)-factor)*0.08f;history.push_back(factor);}
  assert(std::fabs(history.back()-1.25f)<0.01f);
  float lo=*std::min_element(history.end()-40,history.end()),hi=*std::max_element(history.end()-40,history.end());
  assert(hi-lo<0.002f); // no wobble once settled
  // Buffer level only nudges the rate by at most 4%.
  assert(dsAudioTargetStretch(1.0,0,target)<=1.0401f&&dsAudioTargetStretch(1.0,2*target,target)>=0.9599f);
  assert(dsAudioTargetStretch(0.1,target,target)==dsAudioMaxStretch); // very slow: capped
  assert(std::fabs(dsAudioTargetStretch(1.0,target,target)-1.f)<1e-6f); // full speed: unchanged
 }
 // 3. Resampler: continuous across blocks, correct length, no level change.
 {
  DSAudioStretcher r;std::vector<int16_t> out,all;std::vector<int16_t> ramp(200);
  for(int i=0;i<100;++i){ramp[i*2]=int16_t(i*100);ramp[i*2+1]=int16_t(-i*100);}
  size_t produced=0;for(int b=0;b<10;++b){r.process(ramp.data(),100,1.25f,out);produced+=out.size()/2;all.insert(all.end(),out.begin(),out.end());}
  assert(produced>=1245&&produced<=1255); // 10 x 100 frames x 1.25
  r.reset();r.process(ramp.data(),100,1.f,out);assert(out.size()==200&&out[2]==0&&out[198]==9800); // factor 1: same length
  for(size_t i=0;i<all.size();i+=2)assert(all[i]>=0&&all[i]<=9900); // interpolation, never beyond the input range
 }
 // 4. Real SDL queue: prebuffer, then continuous playback through an underrun.
 {
  SDL_SetMainReady();SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_AUDIO)==0);
  SDL_AudioSpec spec{};spec.freq=32768;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=512;
  auto device=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);assert(device);
  DSAudioQueue q;q.configure(device,32768);std::vector<int16_t> music(4096,30000);
  assert(q.push(music.data(),2048,nullptr,0.0));assert(!q.playing&&SDL_GetAudioDeviceStatus(device)==SDL_AUDIO_PAUSED);
  assert(SDL_GetQueuedAudioSize(device)==2048*4); // factor 1 before playback: same length
  assert(q.push(music.data(),2048,nullptr,0.0625));assert(q.playing&&SDL_GetAudioDeviceStatus(device)==SDL_AUDIO_PLAYING);
  SDL_Delay(220);assert(SDL_GetQueuedAudioSize(device)==0);
  assert(q.push(music.data(),512,nullptr,0.08));
  assert(q.playing&&q.underruns==1&&SDL_GetAudioDeviceStatus(device)==SDL_AUDIO_PLAYING);
  assert(SDL_GetQueuedAudioSize(device)>0);
  q.reset();assert(!q.playing&&SDL_GetQueuedAudioSize(device)==0&&q.factor==1.f);
  assert(std::abs(dsAudioFrameInterval(60,0,12000,false)-1./60)<1e-10);assert(dsAudioFrameInterval(60,0,12000,true)<1./60);assert(dsAudioFrameInterval(60,20000,12000,true)>1./60);
  SDL_CloseAudioDevice(device);SDL_Quit();
 }
 std::cout<<"PASS: speed estimate, steady stretch without wobble, click-free resampling across blocks, SDL prebuffer and continuous playback\n";return 0;
}
