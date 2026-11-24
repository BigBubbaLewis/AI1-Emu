#include "library_ui.h"
#include "runtime_paths.h"
#include "app_version.h"
#include "game_database.h"
#include "platform_library.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <cstdio>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 4096
#include "third_party/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"
namespace fs=std::filesystem;
static std::string base;

// v0.4 palette, shared with the AI1 Emu site: ink grounds, electric ultramarine,
// lime / pink / orange accents and one colour per console.
static const SDL_Color INK{14,11,42,255},INK2{23,19,61,255},INK3{33,28,84,255},LINE{56,50,114,255},WHITE{255,255,255,255},
 TEXT{238,235,247,255},MUTED{170,164,200,255},DIM{122,116,162,255},BLUE{28,20,194,255},BLUE2{58,48,255,255},
 LIME{198,244,50,255},PINK{255,61,154,255},ORANGE{255,122,26,255},AQUA{34,230,207,255},SUN{255,210,63,255},
 VIOLET{156,140,255,255},SKY{76,201,255,255},CORAL{255,107,107,255},MINT{61,220,151,255},PEACH{255,179,138,255};
static SDL_Color alpha(SDL_Color c,Uint8 a){c.a=a;return c;}
static SDL_Color shade(SDL_Color c,float k){return {Uint8(std::min(255.f,c.r*k)),Uint8(std::min(255.f,c.g*k)),Uint8(std::min(255.f,c.b*k)),c.a};}
static SDL_Color platformColor(Platform p){
 switch(p){case Platform::GBA:return LIME;case Platform::GB:return MINT;case Platform::GBC:return AQUA;case Platform::NES:return CORAL;
 case Platform::SNES:return VIOLET;case Platform::Genesis:return SKY;case Platform::MasterSystem:return PEACH;case Platform::GameGear:return SUN;
 case Platform::DS:return PINK;case Platform::DSi:return WHITE;case Platform::N64:return ORANGE;default:return MUTED;}
}
static std::string shortTitle(const std::string& title){auto cut=title.find(" (");return cut==std::string::npos?title:title.substr(0,cut);}
static std::string initials(const std::string& title){
 std::string out;bool start=true;
 for(char c:shortTitle(title)){unsigned char u=(unsigned char)c;if(std::isalnum(u)){if(start&&out.size()<2)out+=char(std::toupper(u));start=false;}else if(c==' '||c=='-'||c==':'||c=='.')start=true;}
 return out.empty()?"?":out;
}
static std::string commas(long long n){std::string s=std::to_string(n);for(int i=int(s.size())-3;i>0;i-=3)s.insert(size_t(i),",");return s;}
static std::string bytesLabel(uintmax_t n){char buffer[32];if(n>=1073741824ULL)std::snprintf(buffer,sizeof buffer,"%.1f GB",n/1073741824.0);else if(n>=1048576ULL)std::snprintf(buffer,sizeof buffer,"%.1f MB",n/1048576.0);else if(n>=1024)std::snprintf(buffer,sizeof buffer,"%.0f KB",n/1024.0);else std::snprintf(buffer,sizeof buffer,"%u B",unsigned(n));return buffer;}

static std::string stem(const std::string& p){return fs::path(p).stem().string();}
static bool cachedImageAvailable(const std::string& path){for(const char* ext:{".png",".jpg",".jpeg"}){int w=0,h=0,c=0;if(stbi_info((path+ext).c_str(),&w,&h,&c)&&w>0&&h>0&&w<=2048&&h<=2048)return true;}return false;}
static std::string coverPath(const std::string& p){
 auto platform=detectPlatform(p);std::string slug=platformInfo(platform).slug;auto custom=base+"covers/"+slug+"/"+stem(p);
 static std::map<std::string,std::string> paths;
 auto it=paths.find(p);if(it!=paths.end())return it->second;
 for(const char* ext:{".png",".jpg",".jpeg"})if(fs::exists(custom+ext))return paths[p]=custom;
 if(platform==Platform::GBA)for(const char* ext:{".png",".jpg",".jpeg"})if(fs::exists(base+"covers/"+stem(p)+ext))return paths[p]=base+"covers/"+stem(p);
 auto name=gameTitle(p);for(char& c:name)if(std::string("&*/:`<>?\\|").find(c)!=std::string::npos)c='_';
 if(platform==Platform::GBA&&name=="Metroid - Zero Mission (USA, Australia)")name="Metroid - Zero Mission (USA)";
 if(platform==Platform::GBA&&cachedImageAvailable(base+"artwork-cache/"+name))return paths[p]=base+"artwork-cache/"+name;
 if((platform==Platform::GB||platform==Platform::GBC)&&!cachedImageAvailable(base+"artwork-cache/"+slug+"/"+name)){std::string other=platform==Platform::GB?"gbc":"gb";if(cachedImageAvailable(base+"artwork-cache/"+other+"/"+name))return paths[p]=base+"artwork-cache/"+other+"/"+name;}
 return paths[p]=base+"artwork-cache/"+slug+"/"+name;
}
static std::string metadata(const std::string& path,const std::string& key){
 static std::map<std::string,std::string> cache;auto id=path+"\n"+key;auto it=cache.find(id);if(it!=cache.end())return it->second;
 std::string file=base+"metadata/"+platformInfo(detectPlatform(path)).slug+"/"+stem(path)+".txt";if(detectPlatform(path)==Platform::GBA&&!fs::exists(file))file=base+"metadata/"+stem(path)+".txt";std::ifstream in(file);std::string line;
 while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind(key+"=",0)==0)return cache[id]=line.substr(key.size()+1,512);}return cache[id]="";
}

const int LibraryUI::tabOrder[LibraryUI::tabCount]={LibraryUI::TabGames,LibraryUI::TabMap,LibraryUI::TabControls,LibraryUI::TabSettings,LibraryUI::TabAbout};
int LibraryUI::tabPosition(int id){for(int i=0;i<tabCount;++i)if(tabOrder[i]==id)return i;return 0;}
void LibraryUI::cycleTab(int direction){int position=tabPosition(tab);position=((position+direction)%tabCount+tabCount)%tabCount;tab=tabOrder[position];}
static const char* tabName(int id){switch(id){case LibraryUI::TabGames:return "Games";case LibraryUI::TabMap:return "Map";case LibraryUI::TabControls:return "Controls";case LibraryUI::TabSettings:return "Settings";default:return "About";}}

LibraryUI::LibraryUI(SDL_Renderer* renderer):r(renderer){
 base=retroverseRoot();storageRoot=base;
 const char* files[4]={"Unbounded-ExtraBold.ttf","Geist-Regular.ttf","Geist-SemiBold.ttf","GeistMono-Medium.ttf"};
 std::vector<unsigned char> fallback;{std::ifstream f(retroverseAssets()+"DejaVuSans.ttf",std::ios::binary);fallback={std::istreambuf_iterator<char>(f),{}};}
 for(int i=0;i<4;++i){std::ifstream f(retroverseAssets()+files[i],std::ios::binary);fontData[i]={std::istreambuf_iterator<char>(f),{}};if(fontData[i].empty())fontData[i]=fallback;}
 if(r)SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
}
LibraryUI::~LibraryUI(){
 stopStorageScan();
 for(auto& f:fonts){if(f.second.atlas)SDL_DestroyTexture(f.second.atlas);delete[] static_cast<stbtt_bakedchar*>(f.second.chars);}
 for(auto& a:images)if(a.second.texture)SDL_DestroyTexture(a.second.texture);
}
void LibraryUI::round(int x,int y,int w,int h,int radius,SDL_Color c){
 if(w<=0||h<=0)return;radius=std::min(radius,std::min(w,h)/2);SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
 SDL_Rect middle{x,y+radius,w,h-2*radius};SDL_RenderFillRect(r,&middle);
 for(int row=0;row<radius;++row){int inset=radius-(int)std::sqrt(float(radius*radius-(radius-row-1)*(radius-row-1)));SDL_RenderDrawLine(r,x+inset,y+row,x+w-inset-1,y+row);SDL_RenderDrawLine(r,x+inset,y+h-row-1,x+w-inset-1,y+h-row-1);}
}
void LibraryUI::gradient(int x,int y,int w,int h,int radius,SDL_Color a,SDL_Color b){
 if(w<=0||h<=0)return;radius=std::min(radius,std::min(w,h)/2);
 for(int row=0;row<h;++row){
  float t=h>1?float(row)/float(h-1):0.f;
  SDL_SetRenderDrawColor(r,Uint8(a.r+(b.r-a.r)*t),Uint8(a.g+(b.g-a.g)*t),Uint8(a.b+(b.b-a.b)*t),Uint8(a.a+(b.a-a.a)*t));
  int inset=0;if(row<radius){int d=radius-row-1;inset=radius-(int)std::sqrt(float(radius*radius-d*d));}else if(row>=h-radius){int d=row-(h-radius);inset=radius-(int)std::sqrt(float(radius*radius-d*d));}
  SDL_RenderDrawLine(r,x+inset,y+row,x+w-inset-1,y+row);
 }
}
void LibraryUI::circle(int cx,int cy,int radius,SDL_Color c){if(radius>0)round(cx-radius,cy-radius,radius*2,radius*2,radius,c);}
void LibraryUI::arrow(int cx,int cy,int size,int direction,SDL_Color c){
 // direction: 0 up, 1 right, 2 down, 3 left
 float s=float(size);SDL_FPoint p[3];
 if(direction==0){p[0]={cx-s,cy+s*.6f};p[1]={cx+s,cy+s*.6f};p[2]={float(cx),cy-s*.8f};}
 else if(direction==2){p[0]={cx-s,cy-s*.6f};p[1]={cx+s,cy-s*.6f};p[2]={float(cx),cy+s*.8f};}
 else if(direction==1){p[0]={cx-s*.6f,cy-s};p[1]={cx-s*.6f,cy+s};p[2]={cx+s*.8f,float(cy)};}
 else{p[0]={cx+s*.6f,cy-s};p[1]={cx+s*.6f,cy+s};p[2]={cx-s*.8f,float(cy)};}
 SDL_Vertex v[3];for(int i=0;i<3;++i){v[i].position=p[i];v[i].color=c;v[i].tex_coord={0,0};}
 SDL_RenderGeometry(r,nullptr,v,3,nullptr,0);
}
static std::vector<int> decode(const std::string& value){std::vector<int> out;for(size_t i=0;i<value.size();){unsigned char c=value[i++];int cp=c;if(c>=0xc2&&c<=0xdf&&i<value.size()){cp=(c&31)<<6;cp|=(unsigned char)value[i++]&63;}else if(c>=128){while(i<value.size()&&((unsigned char)value[i]&192)==128)++i;cp='?';}out.push_back(cp>=32&&cp<256?cp:'?');}return out;}
void LibraryUI::stopStorageScan(){
 if(!storageThread)return;bool incomplete=storageState.load()!=2;storageStop=true;SDL_WaitThread(storageThread,nullptr);storageThread=nullptr;
 if(incomplete){std::lock_guard<std::mutex> lock(storageMutex);storage.clear();storageState=0;}storageStop=false;
}
LibraryUI::Font* LibraryUI::font(int family,int size){
 if(family<0||family>3)family=Body;
 if(!r||fontData[family].empty())return nullptr;
 auto& f=fonts[family*1000+size];if(f.atlas)return &f;
 // Smallest atlas that holds glyphs 32-255 for the bundled fonts (measured: <=22px fits
 // 256x256, <=48px fits 512x512); the loop doubles if a bake does not fit.
 for(int atlas=size<=22?256:size<=48?512:size<=96?1024:2048;atlas<=2048;atlas*=2){
  auto* chars=new stbtt_bakedchar[224];std::vector<unsigned char> bitmap(size_t(atlas)*size_t(atlas));
  int fit=stbtt_BakeFontBitmap(fontData[family].data(),0,(float)size,bitmap.data(),atlas,atlas,32,224,chars);
  if(fit<=0&&atlas<2048){delete[] chars;continue;}
  std::vector<Uint32> pixels(bitmap.size());for(size_t i=0;i<pixels.size();++i)pixels[i]=(Uint32(bitmap[i])<<24)|0xffffff;
  f.atlas=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,atlas,atlas);
  if(!f.atlas){delete[] chars;return nullptr;}
  SDL_UpdateTexture(f.atlas,nullptr,pixels.data(),atlas*4);SDL_SetTextureBlendMode(f.atlas,SDL_BLENDMODE_BLEND);
  f.chars=chars;f.height=float(size);f.atlasSize=atlas;return &f;
 }
 return nullptr;
}
int LibraryUI::measure(const std::string& value,int size,int family){
 auto* f=font(family,size);if(!f)return int(value.size()*size*.55f);
 auto* chars=static_cast<stbtt_bakedchar*>(f->chars);float width=0;for(int cp:decode(value))width+=chars[cp-32].xadvance;return int(width+.5f);
}
void LibraryUI::text(std::string value,int x,int y,int size,SDL_Color c,int maxWidth,bool centered,int family){
 auto* f=font(family,size);if(!f)return;
 auto* chars=static_cast<stbtt_bakedchar*>(f->chars);const int atlas=f->atlasSize;auto cps=decode(value);float width=0;for(int cp:cps)width+=chars[cp-32].xadvance;
 if(width>maxWidth){float dots=chars['.'-32].xadvance*3;while(!cps.empty()&&width+dots>maxWidth){width-=chars[cps.back()-32].xadvance;cps.pop_back();}cps.insert(cps.end(),3,'.');width+=dots;}
 float pen=(float)(centered?x+(maxWidth-width)/2:x),baseline=y+size*.8f;
 SDL_SetTextureColorMod(f->atlas,c.r,c.g,c.b);SDL_SetTextureAlphaMod(f->atlas,c.a);
 // An empty intersection would disable clipping in SDL2, so skip text that is fully clipped.
 SDL_Rect oldClip;bool hadClip=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&oldClip);SDL_Rect clip{x,y-size/4,maxWidth,size+size/2+8};if(hadClip&&!SDL_IntersectRect(&clip,&oldClip,&clip))return;SDL_RenderSetClipRect(r,&clip);
 for(int cp:cps){stbtt_aligned_quad q;stbtt_GetBakedQuad(chars,atlas,atlas,cp-32,&pen,&baseline,&q,1);SDL_Rect src{int(q.s0*atlas),int(q.t0*atlas),int((q.s1-q.s0)*atlas+.5f),int((q.t1-q.t0)*atlas+.5f)};SDL_Rect dst{int(q.x0),int(q.y0),src.w,src.h};SDL_RenderCopy(r,f->atlas,&src,&dst);}
 SDL_RenderSetClipRect(r,hadClip?&oldClip:nullptr);
}
LibraryUI::Art& LibraryUI::art(const std::string& path){
 auto it=images.find(path);if(it!=images.end()){it->second.used=tick;return it->second;}
 if(images.size()>=12){auto oldest=std::min_element(images.begin(),images.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});if(oldest->second.texture)SDL_DestroyTexture(oldest->second.texture);images.erase(oldest);}
 Art a;a.used=tick;
 for(const char* ext:{".png",".jpg",".jpeg"}){std::string name=path+ext;std::ifstream file(name,std::ios::binary|std::ios::ate);if(!file||file.tellg()>8*1024*1024)continue;file.close();int channels=0,w=0,h=0;if(!stbi_info(name.c_str(),&w,&h,&channels)||w>2048||h>2048)continue;auto* data=stbi_load(name.c_str(),&w,&h,&channels,4);if(!data)continue;
 SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(data,w,h,32,w*4,SDL_PIXELFORMAT_ABGR8888);
 if(surface){float scale=std::min(1.f,512.f/std::max(w,h));a.w=std::max(1,int(w*scale));a.h=std::max(1,int(h*scale));SDL_Surface* small=SDL_CreateRGBSurfaceWithFormat(0,a.w,a.h,32,SDL_PIXELFORMAT_ABGR8888);if(small){SDL_BlitScaled(surface,nullptr,small,nullptr);a.texture=SDL_CreateTextureFromSurface(r,small);SDL_FreeSurface(small);}SDL_FreeSurface(surface);}stbi_image_free(data);break;
 }return images.emplace(path,a).first->second;
}
void LibraryUI::picture(Art& a,int x,int y,int w,int h,int radius){
 if(!a.texture)return;
 // Center-crop artwork; round only the short edge strips, with one central copy.
 float scale=std::max(float(w)/a.w,float(h)/a.h);float sw=w/scale,sh=h/scale,sx=(a.w-sw)/2,sy=(a.h-sh)/2;
 auto strip=[&](int row,int height,int inset){SDL_Rect src{int(sx+inset/scale),int(sy+row/scale),std::max(1,int((w-2*inset)/scale)),std::max(1,int(height/scale))};SDL_Rect dst{x+inset,y+row,w-2*inset,height};SDL_RenderCopy(r,a.texture,&src,&dst);};
 strip(radius,h-2*radius,0);for(int row=0;row<radius;++row){int inset=radius-int(std::sqrt(float(radius*radius-(radius-row-1)*(radius-row-1))));strip(row,1,inset);strip(h-row-1,1,inset);}
}

// ---------------------------------------------------------------- frame
void LibraryUI::draw(const std::vector<std::string>& games,size_t& selected,bool cartridge,const std::string& status,bool present){
 ++tick;int count=(int)games.size();if(count&&selected>=games.size())selected=0;
 SDL_SetRenderDrawColor(r,INK.r,INK.g,INK.b,255);SDL_RenderClear(r);
 for(int y=0;y<300;y+=3){SDL_SetRenderDrawColor(r,58,48,255,Uint8(40*(1.f-y/300.f)));SDL_Rect band{0,y,1280,3};SDL_RenderFillRect(r,&band);}
 drawHeader(count);drawTabs(cartridge);
 switch(tab){
  case TabGames:drawGames(games,selected,cartridge,status);break;
  case TabMap:drawMap(games,selected);break;
  case TabControls:drawControls();break;
  case TabSettings:drawSettings(cartridge);break;
  default:drawAbout();break;
 }
 drawFooter();
 if(present)SDL_RenderPresent(r);
}
void LibraryUI::drawHeader(int count){
 round(14,12,1252,66,33,BLUE);
 circle(52,45,14,PINK);circle(67,45,14,alpha(LIME,215));circle(60,57,13,alpha(BLUE2,215));
 text("ai1",90,26,27,WHITE,120,false,Display);int w=measure("ai1",27,Display);
 text("emu",92+w,26,27,LIME,140,false,Display);int vx=92+w+measure("emu",27,Display)+14;
 round(vx,32,90,28,14,alpha(WHITE,44));text("v" AI1EMU_VERSION,vx,38,15,WHITE,90,true,Mono);
 std::string label=std::string(category==0?"All Games":platforms()[category-1].shortName)+"  \xC2\xB7  "+std::to_string(count)+(count==1?" game":" games");
 int lw=measure(label,19,BodyBold);text(label,1104-lw,35,19,WHITE,lw+4,false,BodyBold);
 round(1122,30,126,30,15,WHITE);text(updateBusy?"UPDATES":"OFFLINE",1122,37,14,INK,126,true,Mono);
}
void LibraryUI::drawTabs(bool cartridge){
 round(98,92,682,54,27,LINE);round(99,93,680,52,26,INK2);
 for(int i=0;i<tabCount;++i){int id=tabOrder[i],x=104+i*134;bool on=tab==id;if(on)round(x,96,134,46,23,WHITE);text(tabName(id),x,108,19,on?INK:MUTED,134,true,on?BodyBold:Body);}
 circle(66,119,16,INK3);text("L",50,110,15,MUTED,32,true,Mono);circle(812,119,16,INK3);text("R",796,110,15,MUTED,32,true,Mono);
 if(tab==TabGames){
  round(952,95,286,50,25,LINE);round(953,96,284,48,24,INK2);round(cartridge?1095:957,100,138,40,20,WHITE);
  text("Cover",957,109,18,cartridge?MUTED:INK,138,true,cartridge?Body:BodyBold);text("Cartridge",1095,109,18,cartridge?INK:MUTED,138,true,cartridge?BodyBold:Body);
  text("X",916,110,15,DIM,26,true,Mono);
 }
}

// ---------------------------------------------------------------- Games
void LibraryUI::drawGames(const std::vector<std::string>& games,size_t& selected,bool cartridge,const std::string& status){
 int count=(int)games.size();
 const int categoryCount=int(platforms().size())+1;const float chip=1111.f/categoryCount;
 for(int i=0;i<categoryCount;++i){
  int x=int(86+i*chip),w=int(chip)-6;bool on=category==i;SDL_Color pc=i==0?WHITE:platformColor(platforms()[i-1].id);
  round(x,156,w,28,14,on?pc:INK2);
  const char* label=i==0?"All Games":platforms()[i-1].shortName;int lw=measure(label,15,on?BodyBold:Body),dot=(i==0||on)?0:13;int sx=x+(w-lw-dot)/2;
  if(dot)circle(sx+4,170,4,pc);text(label,sx+dot,161,15,on?INK:TEXT,lw+6,false,on?BodyBold:Body);
 }
 text("ZL",44,162,13,DIM,34,true,Mono);text("ZR",1204,162,13,DIM,34,true,Mono);
 Uint32 now=SDL_GetTicks();float dt=scrollTime?float(now-scrollTime)/1000.f:1.f/60.f;scrollTime=now;advanceScroll(games.size(),selected,dt);
 SDL_Rect clip{62,186,1156,236};SDL_RenderSetClipRect(r,&clip);
 int start=std::max(0,int(scroll/225)-1),end=std::min(count,start+8);
 for(int i=start;i<end;++i){
  SDL_RenderSetClipRect(r,&clip);int x=86+i*225-int(scroll);bool active=i==(int)selected;int y=cartridge?222:194,w=202,h=cartridge?154:212,rad=cartridge?30:22;
  auto platform=detectPlatform(games[i]);SDL_Color pc=platformColor(platform);auto title=gameTitle(games[i]);
  round(x-2,y+10,w+4,h+2,rad+2,{0,0,0,90});
  if(active){round(x-8,y-8,w+16,h+16,rad+8,LIME);round(x-4,y-4,w+8,h+8,rad+4,WHITE);}
  auto& cover=art(coverPath(games[i]));
  if(cover.texture){round(x,y,w,h,rad,INK3);picture(cover,x,y,w,h,rad);}
  else{
   gradient(x,y,w,h,rad,shade(pc,.95f),shade(pc,.28f));
   circle(x+w-38,y+38,26,alpha(WHITE,26));
   text(initials(title),x,y+(cartridge?34:58),cartridge?40:48,alpha(WHITE,245),w,true,Display);
   const char* tag=platformInfo(platform).shortName;int tw=measure(tag,12,Mono)+18;round(x+12,y+12,tw,22,11,alpha(INK,200));text(tag,x+12,y+16,12,WHITE,tw,true,Mono);
   round(x+10,y+h-46,w-20,36,14,alpha(INK,175));text(shortTitle(title),x+16,y+h-38,16,WHITE,w-32,true,BodyBold);
  }
  if(cartridge)round(x+44,y+5,w-88,4,2,alpha(WHITE,160));
 }
 SDL_RenderSetClipRect(r,nullptr);
 if(count){circle(36,300,22,INK2);arrow(34,300,9,3,WHITE);circle(1244,300,22,INK2);arrow(1246,300,9,1,WHITE);}
 round(54,432,1172,192,28,LINE);round(55,433,1170,190,27,INK2);
 if(count){
  auto& path=games[selected];auto platform=detectPlatform(path);SDL_Color pc=platformColor(platform);auto title=gameTitle(path);
  auto shotPath=base+"screenshots/"+platformInfo(platform).slug+"/"+stem(path);if(platform==Platform::GBA)for(const char* ext:{".png",".jpg",".jpeg"})if(fs::exists(base+"screenshots/"+stem(path)+ext)){shotPath=base+"screenshots/"+stem(path);break;}
  auto& shot=art(shotPath);
  if(shot.texture)picture(shot,78,452,244,152,18);
  else{auto& cover=art(coverPath(path));if(cover.texture)picture(cover,78,452,244,152,18);else{gradient(78,452,244,152,18,shade(pc,.9f),shade(pc,.3f));text(initials(title),78,490,46,alpha(WHITE,240),244,true,Display);}}
  auto shortName=shortTitle(title);text(shortName,350,452,26,WHITE,500,false,Display);
  if(title.size()>shortName.size()+1)text(title.substr(shortName.size()+1),352,490,14,MUTED,500,false,Mono);
  int cx=350;std::string pname=platformInfo(platform).name;int pw=measure(pname,15,BodyBold)+28;round(cx,516,pw,30,15,pc);text(pname,cx,522,15,INK,pw,true,BodyBold);cx+=pw+8;
  bool known=gameRecognized(path);std::string badge=known?"No-Intro match":"Not in the table";int bw=measure(badge,15,BodyBold)+28;
  round(cx,516,bw,30,15,known?alpha(LIME,46):alpha(WHITE,22));text(badge,cx,522,15,known?LIME:MUTED,bw,true,BodyBold);
  auto description=metadata(path,"description");if(description.empty())description=isDualScreen(platform)?"melonDS software core / performance unverified":"Local game / "+std::string(platformInfo(platform).name);
  text(description,350,558,17,MUTED,500);text(status,350,588,16,SUN,500,false,BodyBold);
  round(876,456,1,144,0,LINE);
  static std::string infoPath,date;static bool saved=false;
  if(infoPath!=path||tick%120==0){infoPath=path;auto saveFile=gameSavePrefix(base,path);saved=fs::exists(saveFile+".sav");if(isDualScreen(platform)&&fs::exists(saveFile+".files")){for(auto& file:fs::directory_iterator(saveFile+".files"))if(file.is_regular_file()&&(file.path().extension()==".srm"||file.path().extension()==".sav")){saved=true;break;}}std::ifstream last(saveFile+".lastplayed");long long stamp=0;last>>stamp;date="Last played: Not recorded";if(stamp>0){std::time_t t=stamp;char buffer[40];auto* tm=std::localtime(&t);if(tm&&std::strftime(buffer,sizeof(buffer),"%b %d, %Y",tm))date="Last played: "+std::string(buffer);}}
  circle(912,478,6,saved?LIME:DIM);text(saved?"Save file: Present":"Saves: Core supported*",928,467,18,WHITE,280);
  circle(912,521,6,platform==Platform::DSi?DIM:AQUA);text(platform==Platform::DSi?"States: Not supported":"States: Core supported*",928,510,18,WHITE,280);
  circle(912,564,6,WHITE);text(date,928,553,17,MUTED,280);
  text("*Game-dependent; unverified",906,592,12,DIM,300,false,Mono);
 }else{
  text("Your game library starts here",86,466,30,WHITE,1100,false,Display);
  text("Add legally supplied games to /switch/ai1emu/roms/",88,526,20,MUTED,1000);
  text("Artwork is optional. AI1 Emu runs offline.",88,564,18,MUTED,1000);
 }
}

// ---------------------------------------------------------------- Map
void LibraryUI::loadMap(){
 mapBlocks.clear();mapKnown=0;
 for(auto& info:platforms()){
  MapBlock block;block.platform=int(info.id);std::ifstream in(retroverseAssets()+info.slug+"-database.tsv");std::string line;int n=0;
  while(std::getline(in,line)){size_t a=line.find('\t'),b=line.find('\t',a==std::string::npos?a:a+1),c=b==std::string::npos?b:line.find('\t',b+1);if(a==std::string::npos||b==std::string::npos||c==std::string::npos)continue;std::string title=line.substr(c+1);if(!title.empty()&&title.back()=='\r')title.pop_back();block.index.emplace(title,n);++n;}
  block.known=n;mapKnown+=n;mapBlocks.push_back(std::move(block));
 }
 layoutMap();
}
void LibraryUI::layoutMap(){
 // Largest dot that fits: for each cell size take the fewest rows whose blocks fit the width.
 const int left=70,width=1140,gap=6,maxHeight=346;mapTop=228;
 int cell=4,rows=0;const int gaps=gap*std::max(0,int(mapBlocks.size())-1);
 for(;cell>=2;--cell){int maxRows=maxHeight/cell;for(int candidate=1;candidate<=maxRows;++candidate){int total=gaps;for(auto& b:mapBlocks)total+=((b.known+candidate-1)/candidate)*cell;if(total<=width){rows=candidate;break;}}if(rows)break;}
 if(!rows){cell=2;rows=maxHeight/2;}
 mapCell=cell;mapHeight=rows*cell;
 int total=gaps;for(auto& b:mapBlocks)total+=((b.known+rows-1)/rows)*cell;int x=left+std::max(0,(width-total)/2);
 for(auto& b:mapBlocks){
  int cols=(b.known+rows-1)/rows;b.x=x;b.w=cols*cell;b.dots.clear();b.dots.reserve(size_t(b.known));
  for(int j=0;j<b.known;++j){int col=j/rows,row=j%rows;b.dots.push_back({x+col*cell,mapTop+mapHeight-(row+1)*cell,cell-1,cell-1});}
  x+=b.w+gap;
 }
}
void LibraryUI::drawMap(const std::vector<std::string>& games,size_t selected){
 round(54,160,1172,472,28,LINE);round(55,161,1170,470,27,INK2);
 if(mapState==0){text("Loading the release tables...",86,380,22,MUTED,1108,true,BodyBold);mapState=1;return;}
 if(mapState==1){loadMap();mapState=2;}
 auto locate=[&](const std::string& game)->std::pair<int,int>{
  auto p=detectPlatform(game);auto title=gameTitle(game);auto find=[&](Platform platform)->std::pair<int,int>{for(size_t i=0;i<mapBlocks.size();++i){auto& b=mapBlocks[i];if(b.platform!=int(platform))continue;auto hit=b.index.find(title);if(hit!=b.index.end())return {int(i),hit->second};}return {-1,-1};};
  auto hit=find(p);if(hit.first<0&&(p==Platform::GB||p==Platform::GBC))hit=find(p==Platform::GB?Platform::GBC:Platform::GB);return hit;
 };
 std::string signature=std::to_string(games.size())+"|"+std::to_string(category)+(games.empty()?std::string():"|"+games.front()+"|"+games.back());
 if(signature!=mapSignature){
  mapSignature=signature;mapOwned.clear();for(auto& b:mapBlocks)b.owned=0;std::set<std::pair<int,int>> marked;
  for(auto& game:games){auto hit=locate(game);if(hit.first>=0&&marked.insert(hit).second){mapOwned.push_back(hit);++mapBlocks[size_t(hit.first)].owned;}}
 }
 text("Every known release",86,178,24,WHITE,640,false,Display);
 std::string own="You own "+commas((long long)mapOwned.size())+" of "+commas(mapKnown);int ow=measure(own,19,BodyBold);
 text(own,1194-ow,184,19,mapOwned.empty()?MUTED:LIME,ow+6,false,BodyBold);
 text("One dot per release in the No-Intro tables bundled with the app. Bright dots are games on this shelf.",88,206,14,MUTED,1100);
 const bool any=!mapOwned.empty();
 for(auto& b:mapBlocks){SDL_Color c=platformColor(Platform(b.platform));SDL_SetRenderDrawColor(r,c.r,c.g,c.b,any?92:170);if(!b.dots.empty())SDL_RenderFillRects(r,b.dots.data(),int(b.dots.size()));}
 for(auto& owned:mapOwned){auto& b=mapBlocks[size_t(owned.first)];const SDL_Rect& d=b.dots[size_t(owned.second)];SDL_Color c=platformColor(Platform(b.platform));SDL_Rect outer{d.x-3,d.y-3,d.w+6,d.h+6};SDL_SetRenderDrawColor(r,c.r,c.g,c.b,255);SDL_RenderFillRect(r,&outer);SDL_Rect inner{d.x-1,d.y-1,d.w+2,d.h+2};SDL_SetRenderDrawColor(r,255,255,255,255);SDL_RenderFillRect(r,&inner);}
 if(!games.empty()&&selected<games.size()){
  auto title=gameTitle(games[selected]);auto hit=locate(games[selected]);
  if(hit.first>=0){auto& b=mapBlocks[size_t(hit.first)];{
   const SDL_Rect& d=b.dots[size_t(hit.second)];int cx=d.x+d.w/2,cy=d.y+d.h/2;float pulse=.5f+.5f*std::sin(tick*.12f);
   circle(cx,cy,10+int(pulse*4),alpha(WHITE,Uint8(50+pulse*70)));circle(cx,cy,5,WHITE);
   std::string label=shortTitle(title);int lw=std::min(1140,measure(label,15,BodyBold)+26);int lx=std::clamp(cx-lw/2,70,1210-lw);int ly=cy-46<mapTop?cy+16:cy-46;
   round(lx,ly,lw,30,15,WHITE);text(label,lx,ly+7,15,INK,lw,true,BodyBold);
  }}
 }
 int lx=72,ly=mapTop+mapHeight+10;
 for(auto& b:mapBlocks){
  const auto& info=platformInfo(Platform(b.platform));std::string t=std::string(info.shortName)+"  "+commas(b.owned)+" / "+commas(b.known);int tw=measure(t,14,b.owned?BodyBold:Body)+18;
  if(lx+tw>1210){lx=72;ly+=22;}
  circle(lx+5,ly+9,5,platformColor(Platform(b.platform)));text(t,lx+16,ly,14,b.owned?WHITE:MUTED,tw,false,b.owned?BodyBold:Body);lx+=tw+16;
 }
}

// ---------------------------------------------------------------- Controls
void LibraryUI::padButton(int cx,int cy,int radius,const char* label,uint32_t bit,bool accent){
 bool on=padHeld&bit;if(on)circle(cx,cy,radius+6,alpha(WHITE,80));
 circle(cx,cy,radius,on?WHITE:(accent?BLUE:INK));
 if(label&&*label)text(label,cx-radius,cy-int(radius*.62f),std::max(12,int(radius*1.05f)),on?INK:WHITE,radius*2,true,BodyBold);
}
void LibraryUI::padPill(int x,int y,int w,int h,const char* label,uint32_t bit){
 bool on=padHeld&bit;if(on)round(x-5,y-5,w+10,h+10,(h+10)/2,alpha(WHITE,80));
 round(x,y,w,h,h/2,on?WHITE:INK);text(label,x,y+int(h*.16f),std::max(12,int(h*.66f)),on?INK:WHITE,w,true,BodyBold);
}
void LibraryUI::drawControls(){
 round(54,160,630,472,28,LINE);round(55,161,628,470,27,INK2);
 text("Try every button",86,180,22,WHITE,560,false,Display);
 const float s=.88f;const int ox=64,oy=206;
 auto X=[&](float v){return ox+int(v*s+.5f);};auto Y=[&](float v){return oy+int(v*s+.5f);};auto S=[&](float v){return int(v*s+.5f);};
 round(X(40),Y(42),S(150),S(252),S(72),PINK);{SDL_Rect square{X(118),Y(42),S(72),S(252)};SDL_SetRenderDrawColor(r,PINK.r,PINK.g,PINK.b,255);SDL_RenderFillRect(r,&square);}
 round(X(490),Y(42),S(150),S(252),S(72),ORANGE);{SDL_Rect square{X(490),Y(42),S(72),S(252)};SDL_SetRenderDrawColor(r,ORANGE.r,ORANGE.g,ORANGE.b,255);SDL_RenderFillRect(r,&square);}
 round(X(196),Y(42),S(288),S(252),S(16),WHITE);round(X(212),Y(58),S(256),S(220),S(12),INK);
 {int w1=measure("ai1",28,Display),w2=measure("emu",28,Display),cx=X(340);text("ai1",cx-(w1+w2)/2,Y(132),28,WHITE,w1+4,false,Display);text("emu",cx-(w1+w2)/2+w1+1,Y(132),28,LIME,w2+4,false,Display);text("CONTROLS",X(212),Y(184),12,MUTED,S(256),true,Mono);}
 padPill(X(62),Y(10),S(62),S(24),"ZL",PadZL);padPill(X(130),Y(18),S(54),S(20),"L",PadL);padPill(X(496),Y(18),S(54),S(20),"R",PadR);padPill(X(556),Y(10),S(62),S(24),"ZR",PadZR);
 padButton(X(170),Y(70),S(13),"-",PadMinus);padButton(X(510),Y(70),S(13),"+",PadPlus);
 auto stick=[&](int cx,int cy,float sx,float sy,uint32_t bit){bool on=(padHeld&bit)||std::fabs(sx)>.4f||std::fabs(sy)>.4f;int radius=S(28);if(on)circle(cx,cy,radius+6,alpha(WHITE,80));circle(cx,cy,radius,INK);circle(cx+int(sx*S(9)),cy+int(sy*S(9)),radius-S(9),on?WHITE:INK3);};
 stick(X(112),Y(118),stickLX,stickLY,PadLStick);stick(X(568),Y(226),stickRX,stickRY,PadRStick);
 struct Dir{float x,y;int direction;uint32_t bit;};const Dir pad[4]={{112,190,0,PadUp},{142,220,1,PadRight},{112,250,2,PadDown},{82,220,3,PadLeft}};
 for(auto& d:pad){padButton(X(d.x),Y(d.y),S(15),"",d.bit);arrow(X(d.x),Y(d.y),S(6),d.direction,(padHeld&d.bit)?INK:WHITE);}
 padButton(X(568),Y(92),S(15),"X",PadX);padButton(X(538),Y(122),S(15),"Y",PadY);padButton(X(598),Y(122),S(15),"A",PadA,true);padButton(X(568),Y(152),S(15),"B",PadB);
 std::string held;const std::pair<uint32_t,const char*> names[]={{PadA,"A"},{PadB,"B"},{PadX,"X"},{PadY,"Y"},{PadL,"L"},{PadR,"R"},{PadZL,"ZL"},{PadZR,"ZR"},{PadPlus,"Plus"},{PadMinus,"Minus"},{PadUp,"Up"},{PadDown,"Down"},{PadLeft,"Left"},{PadRight,"Right"},{PadLStick,"L stick"},{PadRStick,"R stick"}};
 for(auto& n:names)if(padHeld&n.first){if(!held.empty())held+=" + ";held+=n.second;}
 if(std::fabs(stickLX)>.4f||std::fabs(stickLY)>.4f){if(!held.empty())held+=" + ";held+="L stick tilt";}
 if(std::fabs(stickRX)>.4f||std::fabs(stickRY)>.4f){if(!held.empty())held+=" + ";held+="R stick tilt";}
 if(held.empty())text("Press any button.",86,498,20,MUTED,570,false,BodyBold);else text("Holding "+held,86,498,20,LIME,570,false,BodyBold);
 text("B returns to your library. Touch a tab to switch.",88,534,13,DIM,570,false,Mono);
 text("Sticks show their tilt live. Nothing here changes your settings.",88,560,13,DIM,570,false,Mono);
 round(700,160,526,472,28,LINE);round(701,161,524,470,27,INK2);
 struct Row{const char* keys;const char* label;uint32_t bits;};
 static const Row library[]={{"D-pad","Move through the carousel",PadLeft|PadRight|PadUp|PadDown},{"A","Play the selected game",PadA},{"X","Cover or Cartridge view",PadX},{"Y","Sort Title A-Z or Z-A",PadY},{"L  R","Change tab",PadL|PadR},{"ZL  ZR","Change console",PadZL|PadZR},{"+  -","Open Settings",PadPlus|PadMinus},{"B","Back, or exit the app",PadB}};
 static const Row inGame[]={{"+","Start",PadPlus},{"-","Select (where supported)",PadMinus},{"ZR*","Save state (DSi unsupported)",PadZR},{"ZL*","Load a quick state",PadZL},{"+  -","Hold both: back to your library",PadPlus|PadMinus},{"*","N64: hold Minus with ZR / ZL",PadMinus|PadZR|PadZL}};
 auto list=[&](const char* heading,const Row* rows,int n,int y){
  text(heading,726,y,16,LIME,480,false,BodyBold);y+=28;
  for(int i=0;i<n;++i,y+=27){bool on=padHeld&rows[i].bits;if(on)round(714,y-4,498,27,13,WHITE);int kw=measure(rows[i].keys,12,Mono)+16;round(724,y-1,kw,21,10,on?INK:INK3);text(rows[i].keys,724,y+2,12,WHITE,kw,true,Mono);text(rows[i].label,724+kw+12,y,15,on?INK:TEXT,470-kw,false,on?BodyBold:Body);}
  return y;
 };
 int y=list("In the library",library,8,180);list("In a game",inGame,6,y+10);
}

// ---------------------------------------------------------------- Settings
void LibraryUI::drawSettings(bool cartridge){
 round(54,160,1172,472,28,LINE);round(55,161,1170,470,27,INK2);
 text("Library settings",86,184,26,WHITE,700,false,Display);text("View and sort apply to this session.",88,230,16,MUTED,700);
 auto toggle=[&](int y,const char* first,const char* second,bool useSecond){round(820,y,372,50,25,INK3);round(useSecond?1008:824,y+4,180,42,21,WHITE);text(first,824,y+14,18,useSecond?MUTED:INK,180,true,useSecond?Body:BodyBold);text(second,1008,y+14,18,useSecond?INK:MUTED,180,true,useSecond?BodyBold:Body);};
 text("Default library view",88,290,22,WHITE,600,false,BodyBold);text("A or X to change",88,324,14,DIM,400,false,Mono);toggle(288,"Cover","Cartridge",cartridge);
 round(86,370,1108,1,0,LINE);
 text("Sort order",88,396,22,WHITE,600,false,BodyBold);text("Y to change",88,430,14,DIM,400,false,Mono);toggle(394,"Title A - Z","Title Z - A",reverse);
 std::string button=updateButton;if(button.size()>5&&button.compare(button.size()-5,5," (ZR)")==0)button.resize(button.size()-5);
 round(86,498,1108,64,22,updateBusy?alpha(LIME,46):INK3);round(102,514,44,32,16,LIME);text("ZR",102,521,14,INK,44,true,BodyBold);text(button,162,515,20,WHITE,1010,false,BodyBold);
 text(updateStatus,90,572,15,MUTED,1090);text("Updates use Wi-Fi only when requested. Gameplay stays offline.",90,602,13,DIM,1090,false,Mono);
}

// ---------------------------------------------------------------- About + SD breakdown
static int storageWorker(void* data){
 auto* ui=static_cast<LibraryUI*>(data);const std::string root=ui->storageRoot;std::vector<LibraryUI::StorageItem> out;
 auto stopped=[ui]{return ui->storageStop.load(std::memory_order_relaxed);};
 auto walk=[&stopped](const fs::path& dir,const std::function<void(const fs::path&,uintmax_t)>& visit){
  std::error_code error;if(stopped()||!fs::exists(dir,error))return;
  fs::recursive_directory_iterator it(dir,fs::directory_options::skip_permission_denied,error),end;
  for(;!error&&it!=end&&!stopped();it.increment(error)){std::error_code inner;if(it->is_regular_file(inner)){auto n=it->file_size(inner);if(!inner)visit(it->path(),n);}}
 };
 std::map<std::string,uintmax_t> roms;uintmax_t otherRoms=0;
 {std::error_code error;fs::directory_iterator it(root+"roms",fs::directory_options::skip_permission_denied,error),end;
  for(;!error&&it!=end&&!stopped();it.increment(error)){std::error_code inner;
   if(it->is_directory(inner)){uintmax_t total=0;walk(it->path(),[&](const fs::path&,uintmax_t n){total+=n;});roms[it->path().filename().string()]+=total;}
   else if(it->is_regular_file(inner)){auto n=it->file_size(inner);if(inner)continue;auto ext=it->path().extension().string();for(char& c:ext)c=char(std::tolower((unsigned char)c));if(ext==".gba")roms["gba"]+=n;else otherRoms+=n;}}}
 for(auto& info:platforms()){auto found=roms.find(info.slug);if(found!=roms.end()&&found->second){out.push_back({std::string("ROMs: ")+info.shortName,found->second,platformColor(info.id)});roms.erase(found);}}
 for(auto& rest:roms)otherRoms+=rest.second;if(otherRoms)out.push_back({"ROMs: other",otherRoms,MUTED});
 uintmax_t saves=0,states=0;walk(root+"saves",[&](const fs::path& p,uintmax_t n){if(p.extension()==".state")states+=n;else saves+=n;});
 if(saves)out.push_back({"Saves",saves,AQUA});if(states)out.push_back({"Quick states",states,VIOLET});
 uintmax_t artwork=0;for(const char* dir:{"covers","artwork-cache","artwork"})walk(root+dir,[&](const fs::path&,uintmax_t n){artwork+=n;});if(artwork)out.push_back({"Covers & artwork",artwork,SUN});
 const std::pair<const char*,std::pair<const char*,SDL_Color>> singles[]={{"screenshots",{"Screenshots",PEACH}},{"metadata",{"Metadata",SKY}},{"system",{"BIOS & firmware",MINT}}};
 for(auto& single:singles){uintmax_t total=0;walk(root+single.first,[&](const fs::path&,uintmax_t n){total+=n;});if(total)out.push_back({single.second.first,total,single.second.second});}
#ifndef RETROVERSE_WINDOWS_PREVIEW
 uintmax_t cores=0;walk("sdmc:/switch/ai1emu/cores",[&](const fs::path&,uintmax_t n){cores+=n;});if(cores)out.push_back({"Emulator cores",cores,BLUE2});
 {std::error_code error;auto n=fs::file_size("sdmc:/switch/ai1emu/AI1Emu.nro",error);if(!error&&n)out.push_back({"AI1 Emu app",n,PINK});}
#endif
 std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.bytes>b.bytes;});
 {std::lock_guard<std::mutex> lock(ui->storageMutex);ui->storage=std::move(out);}
 ui->storageState=2;return 0;
}
struct TreeBox{double x,y,w,h;int index;};
static std::vector<TreeBox> squarify(const std::vector<double>& values,double X,double Y,double W,double H){
 std::vector<TreeBox> out;double total=0;for(double v:values)total+=v;if(total<=0||W<=0||H<=0)return out;
 double scale=W*H/total,x=X,y=Y,w=W,h=H;std::vector<std::pair<double,int>> rest,row;
 for(size_t i=0;i<values.size();++i)if(values[i]>0)rest.push_back({values[i]*scale,int(i)});
 auto worst=[](const std::vector<std::pair<double,int>>& items,double side){double sum=0,most=0,least=1e300;for(auto& q:items){sum+=q.first;most=std::max(most,q.first);least=std::min(least,q.first);}if(sum<=0||side<=0)return 1e300;return std::max(side*side*most/(sum*sum),(sum*sum)/(side*side*least));};
 auto flush=[&]{double sum=0;for(auto& q:row)sum+=q.first;
  if(w>=h){double rw=sum/h,cy=y;for(auto& q:row){double rh=q.first/rw;out.push_back({x,cy,rw,rh,q.second});cy+=rh;}x+=rw;w-=rw;}
  else{double rh=sum/w,cx=x;for(auto& q:row){double rw=q.first/rh;out.push_back({cx,y,rw,rh,q.second});cx+=rw;}y+=rh;h-=rh;}
  row.clear();};
 size_t k=0;while(k<rest.size()){double side=std::min(w,h);auto candidate=row;candidate.push_back(rest[k]);if(row.empty()||worst(candidate,side)<=worst(row,side)){row=candidate;++k;}else flush();}
 if(!row.empty())flush();return out;
}
void LibraryUI::drawAbout(){
 round(54,160,562,472,28,LINE);round(55,161,560,470,27,INK2);
 circle(102,214,20,PINK);circle(122,214,20,alpha(LIME,215));circle(112,232,19,alpha(BLUE2,215));
 text("ai1",152,196,30,WHITE,140,false,Display);int w=measure("ai1",30,Display);text("emu",154+w,196,30,LIME,160,false,Display);
 text("All In 1 Emulator v" AI1EMU_VERSION,154,238,17,MUTED,430);
 const char* cores[]={"mGBA","FCEUmm","Snes9x","Genesis Plus GX","melonDS","Mupen64Plus-Next"};int cx=86,cy=282;
 for(auto* core:cores){int cw=measure(core,14,BodyBold)+26;if(cx+cw>590){cx=86;cy+=36;}round(cx,cy,cw,28,14,INK3);text(core,cx,cy+6,14,TEXT,cw,true,BodyBold);cx+=cw+8;}
 int ty=cy+54;
 text("Offline. Normal clocks.",88,ty,17,WHITE,500);text("In game: hold Plus + Minus to return.",88,ty+30,17,WHITE,500);
 text("States: ZR / ZL. N64: also hold Minus.",88,ty+60,17,WHITE,500);text("Battery saves are automatic.",88,ty+90,17,WHITE,500);
 text("Hardware gameplay and save reliability remain unverified.",88,ty+128,15,SUN,510,false,BodyBold);
 text("Unbounded + Geist fonts: SIL OFL. stb: MIT or public domain.",88,ty+162,12,DIM,520,false,Mono);
 round(630,160,596,472,28,LINE);round(631,161,594,470,27,INK2);
 text("Your SD card",662,184,24,WHITE,360,false,Display);
 if(storageState==0&&!storageThread){storageState=1;storageThread=SDL_CreateThread(storageWorker,"ai1-storage",this);if(!storageThread)storageState=2;}
 if(storageState==2&&storageThread){SDL_WaitThread(storageThread,nullptr);storageThread=nullptr;}
 std::vector<StorageItem> items;{std::lock_guard<std::mutex> lock(storageMutex);items=storage;}
 const double TX=662,TY=236,TW=532,TH=330;
 if(storageState!=2){std::string dots(size_t(1+(tick/20)%3),'.');text("Measuring your library"+dots,662,380,20,MUTED,532,true,BodyBold);}
 else if(items.empty()){text("Nothing to measure yet. Add games to /switch/ai1emu/roms/.",662,380,17,MUTED,532,true);}
 else{
  uintmax_t total=0;std::vector<double> values;for(auto& item:items){total+=item.bytes;values.push_back(double(item.bytes));}
  std::string sum="Total "+bytesLabel(total);int sw=measure(sum,18,BodyBold);text(sum,1194-sw,190,18,LIME,sw+6,false,BodyBold);
  for(auto& box:squarify(values,TX,TY,TW,TH)){
   auto& item=items[size_t(box.index)];int bx=int(box.x+.5),by=int(box.y+.5),bw=int(box.x+box.w+.5)-bx,bh=int(box.y+box.h+.5)-by;
   if(bw<8||bh<8)continue; // too small to read; the total still includes it
   round(bx+2,by+2,bw-4,bh-4,std::min(14,std::min(bw,bh)/3),item.color);
   bool dark=item.color.r+item.color.g+item.color.b<300;SDL_Color ink=dark?WHITE:INK;
   if(bw>=104&&bh>=48){text(item.name,bx+12,by+10,14,ink,bw-22,false,BodyBold);text(bytesLabel(item.bytes),bx+12,by+30,13,ink,bw-22,false,Mono);}
   else if(bw>=56&&bh>=26)text(bytesLabel(item.bytes),bx+8,by+6,12,ink,bw-12,false,Mono);
  }
 }
 text("Measured once per session under "+storageRoot,662,590,12,DIM,532,false,Mono);
}

// ---------------------------------------------------------------- footer
void LibraryUI::drawFooter(){
 round(0,652,1280,68,0,{9,7,28,255});round(0,651,1280,1,0,LINE);
 text("ai1",40,673,17,WHITE,60,false,Display);int w=measure("ai1",17,Display);text("emu",41+w,673,17,LIME,60,false,Display);
 text("v" AI1EMU_VERSION,52+w+measure("emu",17,Display),677,13,DIM,90,false,Mono);
 struct Hint{const char* key;const char* label;};
 static const Hint games[5]={{"A","Play"},{"X","Change View"},{"Y","Sort"},{"B","Back"},{"+","Settings"}};
 static const Hint settings[5]={{"A","View"},{"Y","Sort"},{"ZR","Update"},{"B","Back"},{nullptr,nullptr}};
 static const Hint map[5]={{"<","Previous"},{">","Next"},{nullptr,nullptr},{"B","Back"},{"+","Settings"}};
 static const Hint controls[5]={{nullptr,nullptr},{nullptr,nullptr},{nullptr,nullptr},{"B","Back"},{nullptr,nullptr}};
 static const Hint about[5]={{nullptr,nullptr},{nullptr,nullptr},{nullptr,nullptr},{"B","Back"},{"+","Settings"}};
 const Hint* hints=tab==TabGames?games:tab==TabSettings?settings:tab==TabMap?map:tab==TabControls?controls:about;
 const int pos[5]={600,704,877,985,1092};
 for(int i=0;i<5;++i){
  if(!hints[i].key)continue;bool wide=std::string(hints[i].key).size()>1;int gw=wide?40:28;bool primary=i==0&&(tab==TabGames||tab==TabMap);
  round(pos[i],672,gw,28,14,primary?LIME:WHITE);
  if(hints[i].key[0]=='<'||hints[i].key[0]=='>')arrow(pos[i]+gw/2,686,6,hints[i].key[0]=='<'?3:1,INK);else text(hints[i].key,pos[i],677,wide?14:17,INK,gw,true,BodyBold);
  text(hints[i].label,pos[i]+gw+8,676,18,WHITE,i==1?130:96);
 }
 if(tab==TabControls)text("Press any button to see it light up.",300,676,18,MUTED,640,false,Body);
}
