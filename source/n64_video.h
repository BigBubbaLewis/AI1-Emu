// SPDX-License-Identifier: MIT
#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <string>
#include <vector>
class N64Video {
public:
 bool open(bool coreProfile,std::string& error);
 void close();
 bool present(unsigned width,unsigned height,bool bottomLeft=true,float aspect=4.f/3.f);
 void status(const char* message);
 // In-game menu support: draw with overlayRenderer(), then showOverlay() puts it on screen.
 SDL_Renderer* overlayRenderer()const{return statusRenderer;}
 void showOverlay();
 // Copies the last presented game picture (for save-state pictures and the menu background).
 bool grab(std::vector<Uint32>& argb,int& w,int& h);
 bool stretch=false;
 uintptr_t framebuffer()const{return fbo;}
 static void (*procedure(const char* name))(){return reinterpret_cast<void(*)()>(SDL_GL_GetProcAddress(name));}
 const char* version()const{return reinterpret_cast<const char*>(glGetString(GL_VERSION));}
private:
 static constexpr int width=1280,height=960;
 SDL_Window* window=nullptr;SDL_GLContext context=nullptr;
 SDL_Surface* statusSurface=nullptr;SDL_Renderer* statusRenderer=nullptr;
 GLuint fbo=0,color=0,depth=0;
 unsigned lastW=0,lastH=0;bool lastBottomLeft=true;
 PFNGLGENFRAMEBUFFERSPROC GenFramebuffers=nullptr;PFNGLBINDFRAMEBUFFERPROC BindFramebuffer=nullptr;
 PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers=nullptr;PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus=nullptr;
 PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D=nullptr;PFNGLGENRENDERBUFFERSPROC GenRenderbuffers=nullptr;
 PFNGLBINDRENDERBUFFERPROC BindRenderbuffer=nullptr;PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage=nullptr;
 PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer=nullptr;PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers=nullptr;
 PFNGLBLITFRAMEBUFFERPROC BlitFramebuffer=nullptr;
};
