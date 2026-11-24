// SPDX-License-Identifier: MIT
#define SDL_MAIN_HANDLED
#include "n64_video.h"
#include <cassert>
#include <cstdio>
int main(){SDL_SetMainReady();assert(SDL_Init(SDL_INIT_VIDEO)==0);N64Video v;std::string error;if(!v.open(true,error)){fprintf(stderr,"%s\n",error.c_str());return 1;}printf("OpenGL: %s\n",v.version());v.status("N64 GRAPHICS CHECK");auto bind=reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBindFramebuffer"));bind(GL_FRAMEBUFFER,GLuint(v.framebuffer()));glClearColor(.2f,.4f,.6f,1);glClear(GL_COLOR_BUFFER_BIT);unsigned char pixel[4]{};glReadPixels(100,100,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]>=50&&pixel[0]<=52&&pixel[1]>=101&&pixel[1]<=103&&pixel[2]>=152&&pixel[2]<=154);assert(v.present(320,240));assert(!v.present(2000,2000));v.close();SDL_Quit();puts("PASS: actual SDL OpenGL context, FBO color readback, presentation and size guard");return 0;}
