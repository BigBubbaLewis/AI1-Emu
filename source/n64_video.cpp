// SPDX-License-Identifier: MIT
#include "n64_video.h"
#include "core_status.h"
#include <algorithm>
#include <cmath>
bool N64Video::open(bool coreProfile,std::string& error){
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,coreProfile?SDL_GL_CONTEXT_PROFILE_CORE:SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
 SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
 window=SDL_CreateWindow("AI1 Emu - Nintendo 64",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
 if(!window||(context=SDL_GL_CreateContext(window))==nullptr){error=SDL_GetError();close();return false;}
 SDL_GL_MakeCurrent(window,context);SDL_GL_SetSwapInterval(0);
#define LOAD(name) name=reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name));if(!name){error="Missing OpenGL function gl" #name;close();return false;}
 LOAD(GenFramebuffers);LOAD(BindFramebuffer);LOAD(DeleteFramebuffers);LOAD(CheckFramebufferStatus);LOAD(FramebufferTexture2D);LOAD(GenRenderbuffers);LOAD(BindRenderbuffer);LOAD(RenderbufferStorage);LOAD(FramebufferRenderbuffer);LOAD(DeleteRenderbuffers);LOAD(BlitFramebuffer);
#undef LOAD
 GenFramebuffers(1,&fbo);BindFramebuffer(GL_FRAMEBUFFER,fbo);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 GenRenderbuffers(1,&depth);BindRenderbuffer(GL_RENDERBUFFER,depth);RenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,width,height);FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
 if(CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){error="Nintendo 64 framebuffer is incomplete";close();return false;}
 statusSurface=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ARGB8888);if(statusSurface)statusRenderer=SDL_CreateSoftwareRenderer(statusSurface);
 if(!statusRenderer){error=SDL_GetError();close();return false;}glBindTexture(GL_TEXTURE_2D,0);BindRenderbuffer(GL_RENDERBUFFER,0);return true;
}
bool N64Video::present(unsigned w,unsigned h,bool bottomLeft,float aspect){
 if(!context||!w||!h||w>width||h>height)return false;
 GLint read=0,draw=0;GLfloat clear[4];GLboolean mask[4];glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetBooleanv(GL_COLOR_WRITEMASK,mask);bool scissor=glIsEnabled(GL_SCISSOR_TEST);
 BindFramebuffer(GL_READ_FRAMEBUFFER,fbo);BindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
 int screenW=1280,screenH=720;SDL_GL_GetDrawableSize(window,&screenW,&screenH);if(!(aspect>0))aspect=float(w)/h;
 int outW=std::min(screenW,int(screenH*aspect)),outH=std::min(screenH,int(outW/aspect)),x=(screenW-outW)/2,y=(screenH-outH)/2;
 if(stretch&&!(w==1280&&h==720)){outW=screenW;outH=screenH;x=y=0;}
 if(!(w==1280&&h==720&&!bottomLeft)){lastW=w;lastH=h;lastBottomLeft=bottomLeft;}
 BlitFramebuffer(0,bottomLeft?0:int(h),int(w),bottomLeft?int(h):0,x,y,x+outW,y+outH,GL_COLOR_BUFFER_BIT,GL_NEAREST);
 BindFramebuffer(GL_READ_FRAMEBUFFER,read);BindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glClearColor(clear[0],clear[1],clear[2],clear[3]);glColorMask(mask[0],mask[1],mask[2],mask[3]);if(scissor)glEnable(GL_SCISSOR_TEST);
 SDL_GL_SwapWindow(window);return true;
}
void N64Video::status(const char* message){if(!statusRenderer)return;coreStatus(statusRenderer,message);glBindTexture(GL_TEXTURE_2D,color);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1280,720,GL_BGRA,GL_UNSIGNED_BYTE,statusSurface->pixels);glBindTexture(GL_TEXTURE_2D,0);present(1280,720,false,16.f/9.f);}
void N64Video::showOverlay(){if(!statusRenderer)return;glBindTexture(GL_TEXTURE_2D,color);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1280,720,GL_BGRA,GL_UNSIGNED_BYTE,statusSurface->pixels);glBindTexture(GL_TEXTURE_2D,0);bool keep=stretch;stretch=false;present(1280,720,false,16.f/9.f);stretch=keep;}
bool N64Video::grab(std::vector<Uint32>& argb,int& w,int& h){
 if(!context||!lastW||!lastH)return false;w=int(lastW);h=int(lastH);std::vector<Uint32> rgba(size_t(w)*h);
 GLint read=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);BindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());BindFramebuffer(GL_READ_FRAMEBUFFER,read);
 argb.resize(rgba.size());
 for(int y=0;y<h;++y){int src=lastBottomLeft?h-1-y:y;for(int x=0;x<w;++x){Uint32 p=rgba[size_t(src)*w+x];Uint32 r=p&0xff,g=(p>>8)&0xff,b=(p>>16)&0xff;argb[size_t(y)*w+x]=0xff000000u|(r<<16)|(g<<8)|b;}}
 return true;
}
void N64Video::close(){
 if(context){if(depth&&DeleteRenderbuffers)DeleteRenderbuffers(1,&depth);if(color)glDeleteTextures(1,&color);if(fbo&&DeleteFramebuffers)DeleteFramebuffers(1,&fbo);}
 fbo=color=depth=0;if(statusRenderer)SDL_DestroyRenderer(statusRenderer);if(statusSurface)SDL_FreeSurface(statusSurface);statusRenderer=nullptr;statusSurface=nullptr;
 if(context)SDL_GL_DeleteContext(context);context=nullptr;if(window)SDL_DestroyWindow(window);window=nullptr;
}
