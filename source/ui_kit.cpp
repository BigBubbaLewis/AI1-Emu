// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
#include "ui_kit.h"
#include "runtime_paths.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 4096
#include "third_party/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

namespace ui {

// ------------------------------------------------------------------ colours
SDL_Color rgba(Uint32 rgb, Uint8 a) { return {Uint8(rgb >> 16), Uint8(rgb >> 8), Uint8(rgb), a}; }
SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
 t = std::clamp(t, 0.f, 1.f);
 auto m = [t](Uint8 x, Uint8 y) { return Uint8(x + (y - x) * t + .5f); };
 return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}
SDL_Color alpha(SDL_Color c, Uint8 a) { c.a = a; return c; }
bool parseHex(const std::string& hex, SDL_Color& out) {
 std::string h = hex;
 if (!h.empty() && h[0] == '#') h.erase(0, 1);
 if (h.size() != 6) return false;
 for (char c : h) if (!std::isxdigit((unsigned char)c)) return false;
 out = rgba(Uint32(std::stoul(h, nullptr, 16)));
 return true;
}
std::string toHex(SDL_Color c) { char b[8]; std::snprintf(b, sizeof b, "#%02X%02X%02X", c.r, c.g, c.b); return b; }
SDL_Color onColor(SDL_Color f) { float l = .299f * f.r + .587f * f.g + .114f * f.b; return l > 165 ? rgba(0x1C1D21) : rgba(0xFFFFFF); }

const std::vector<Theme>& presetThemes() {
 static const std::vector<Theme> themes = {
  {"white", "White", rgba(0xEDEDF0), rgba(0xFFFFFF), rgba(0xE4E5E9), rgba(0xD6D7DC), rgba(0x2B2C30), rgba(0x7C7D84), rgba(0x1E9BE6), rgba(0xFFFFFF), rgba(0x2B2C30), rgba(0xFFFFFF), false},
  {"black", "Black", rgba(0x2A2A2D), rgba(0x38383C), rgba(0x46464B), rgba(0x4C4C51), rgba(0xF5F5F7), rgba(0xA3A3AA), rgba(0x2EC4F0), rgba(0x10232B), rgba(0xF5F5F7), rgba(0x2A2A2D), true},
  {"ocean", "Ocean", rgba(0xE4EEF5), rgba(0xFFFFFF), rgba(0xD5E3EE), rgba(0xC6D7E4), rgba(0x1F3140), rgba(0x6A8092), rgba(0x1A7FD1), rgba(0xFFFFFF), rgba(0x1F3140), rgba(0xFFFFFF), false},
  {"sakura", "Sakura", rgba(0xF6E9EE), rgba(0xFFFFFF), rgba(0xEEDCE3), rgba(0xE3CDD6), rgba(0x3A2430), rgba(0x8E6E7C), rgba(0xE0517F), rgba(0xFFFFFF), rgba(0x3A2430), rgba(0xFFFFFF), false},
  {"forest", "Forest", rgba(0xE6EEE7), rgba(0xFFFFFF), rgba(0xD6E3D8), rgba(0xC7D7CA), rgba(0x22332A), rgba(0x6C8274), rgba(0x2E9E5B), rgba(0xFFFFFF), rgba(0x22332A), rgba(0xFFFFFF), false},
  {"night", "Night", rgba(0x16181E), rgba(0x22252D), rgba(0x2E323C), rgba(0x343844), rgba(0xF2F3F6), rgba(0x9096A3), rgba(0xFFB43C), rgba(0x2A1C00), rgba(0xF2F3F6), rgba(0x16181E), true},
 };
 return themes;
}
const Theme& presetTheme(const std::string& id) {
 for (auto& t : presetThemes()) if (t.id == id) return t;
 return presetThemes().front();
}

const char* fontFamilyName(int f) { return f == FontRubik ? "Rubik" : f == FontNunito ? "Nunito" : "Rounded"; }

SDL_Color consoleColor(int p) {
 static const Uint32 colors[] = {0x7B5CD6, 0x4C9A5E, 0x1FA5A0, 0xD9463B, 0x5B6EE1, 0x2F80D1, 0xE0663A, 0x2FB3C9, 0xE0517F, 0x8A8F99, 0xE9A23B, 0xB03A8C};
 if (p < 0 || p >= int(sizeof colors / sizeof colors[0])) return rgba(0x8A8F99);
 return rgba(colors[p]);
}

std::string initials(const std::string& title) {
 std::string t = title.substr(0, title.find(" (")), out;
 bool start = true;
 for (char c : t) {
  unsigned char u = (unsigned char)c;
  if (std::isalnum(u)) { if (start && out.size() < 2) out += char(std::toupper(u)); start = false; }
  else if (c == ' ' || c == '-' || c == ':' || c == '.') start = true;
 }
 return out.empty() ? "?" : out;
}

std::string prettyTitle(const std::string& title) {
 std::string s = title.substr(0, title.find(" ("));
 for (const char* article : {"The", "A", "An"}) {
  std::string tag = std::string(", ") + article;
  auto pos = s.find(tag);
  if (pos == std::string::npos) continue;
  size_t end = pos + tag.size();
  if (end == s.size() || s.compare(end, 3, " - ") == 0 || s[end] == ':') { s = std::string(article) + " " + s.substr(0, pos) + s.substr(end); break; }
 }
 return s;
}

// ------------------------------------------------------------------ painter
static float coverage(float px, float py, float cx, float cy, float r) {
 int inside = 0;
 for (int sy = 0; sy < 4; ++sy)
  for (int sx = 0; sx < 4; ++sx) {
   float x = px + (sx + .5f) / 4.f - cx, y = py + (sy + .5f) / 4.f - cy;
   if (x * x + y * y <= r * r) ++inside;
  }
 return inside / 16.f;
}

static SDL_Texture* alphaTexture(SDL_Renderer* r, int w, int h, const std::vector<float>& a) {
 std::vector<Uint32> px(size_t(w) * h);
 for (size_t i = 0; i < px.size(); ++i) px[i] = (Uint32(std::clamp(a[i], 0.f, 1.f) * 255.f + .5f) << 24) | 0xFFFFFF;
 SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
 if (!t) return nullptr;
 SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
 SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
 SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
 return t;
}

Painter::Painter(SDL_Renderer* renderer) : r(renderer), theme(presetThemes().front()) {
 std::ifstream f(retroverseAssets() + "DejaVuSans.ttf", std::ios::binary);
 fallback = {std::istreambuf_iterator<char>(f), {}};
 if (!r) return;
 SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
 std::vector<float> a(128 * 128);
 for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) a[size_t(y) * 128 + x] = coverage(float(x), float(y), 64, 64, 64);
 disc = alphaTexture(r, 128, 128, a);
 std::vector<float> b(64 * 64);
 for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) b[size_t(y) * 64 + x] = 1.f - coverage(float(x), float(y), 64, 64, 64);
 inverse = alphaTexture(r, 64, 64, b);
}

Painter::~Painter() {
 for (auto& f : fonts) { if (f.second.atlas) SDL_DestroyTexture(f.second.atlas); delete[] static_cast<stbtt_packedchar*>(f.second.chars); }
 for (auto& i : icons) if (i.second) SDL_DestroyTexture(i.second);
 if (disc) SDL_DestroyTexture(disc);
 if (inverse) SDL_DestroyTexture(inverse);
}

int Painter::radius(int base) const {
 if (corners <= 0) return std::max(2, base * 3 / 10);
 if (corners >= 2) return base * 8 / 5;
 return base;
}

const std::vector<unsigned char>& Painter::data(int fam, int weight) {
 if (weight == Mono) fam = 0;
 fam = std::clamp(fam, 0, FontFamilyCount - 1);
 if (!fontLoaded[fam][weight]) {
  static const char* files[FontFamilyCount][4] = {
   {"Rounded-Medium.ttf", "Rounded-Bold.ttf", "Rounded-ExtraBold.ttf", "GeistMono-Medium.ttf"},
   {"Rubik-Medium.ttf", "Rubik-Bold.ttf", "Rubik-ExtraBold.ttf", "GeistMono-Medium.ttf"},
   {"Nunito-SemiBold.ttf", "Nunito-ExtraBold.ttf", "Nunito-Black.ttf", "GeistMono-Medium.ttf"}};
  std::ifstream f(retroverseAssets() + files[fam][weight], std::ios::binary);
  fontData[fam][weight] = {std::istreambuf_iterator<char>(f), {}};
  fontLoaded[fam][weight] = true;
 }
 return fontData[fam][weight].empty() ? fallback : fontData[fam][weight];
}

static const int extraCodepoints[] = {0x2022, 0x2026, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D};
static constexpr int extraCount = int(sizeof extraCodepoints / sizeof extraCodepoints[0]);
static constexpr int glyphCount = 224 + extraCount;

Painter::Font* Painter::font(int weight, int size) {
 if (!r || size <= 0) return nullptr;
 weight = std::clamp(weight, 0, 3);
 int fam = weight == Mono ? 0 : family;
 long key = (long(fam) * 8 + weight) * 1000 + size;
 auto& f = fonts[key];
 if (f.atlas) return &f;
 const auto& bytes = data(fam, weight);
 if (bytes.empty()) return nullptr;
 int over = size <= 36 ? 2 : 1;
 for (int atlas = size <= 20 ? 256 : size <= 44 ? 512 : 1024; atlas <= 4096; atlas *= 2) {
  std::vector<unsigned char> bitmap(size_t(atlas) * atlas);
  auto* chars = new stbtt_packedchar[glyphCount];
  stbtt_pack_context pc;
  if (!stbtt_PackBegin(&pc, bitmap.data(), atlas, atlas, 0, 1, nullptr)) { delete[] chars; return nullptr; }
  stbtt_PackSetOversampling(&pc, over, 1);
  stbtt_pack_range ranges[2] = {};
  ranges[0].font_size = float(size); ranges[0].first_unicode_codepoint_in_range = 32; ranges[0].num_chars = 224; ranges[0].chardata_for_range = chars;
  ranges[1].font_size = float(size); ranges[1].array_of_unicode_codepoints = const_cast<int*>(extraCodepoints); ranges[1].num_chars = extraCount; ranges[1].chardata_for_range = chars + 224;
  int ok = stbtt_PackFontRanges(&pc, bytes.data(), 0, ranges, 2);
  stbtt_PackEnd(&pc);
  if (!ok && atlas < 4096) { delete[] chars; continue; }
  std::vector<Uint32> px(bitmap.size());
  for (size_t i = 0; i < px.size(); ++i) px[i] = (Uint32(bitmap[i]) << 24) | 0xFFFFFF;
  f.atlas = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, atlas, atlas);
  if (!f.atlas) { delete[] chars; return nullptr; }
  SDL_UpdateTexture(f.atlas, nullptr, px.data(), atlas * 4);
  SDL_SetTextureBlendMode(f.atlas, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(f.atlas, SDL_ScaleModeLinear);
  f.chars = chars; f.atlasSize = atlas; f.size = size;
  return &f;
 }
 return nullptr;
}

static std::vector<int> decode(const std::string& s) {
 std::vector<int> out;
 for (size_t i = 0; i < s.size();) {
  unsigned char c = (unsigned char)s[i++];
  int cp = c;
  if (c >= 0xC0 && c <= 0xDF && i < s.size()) cp = ((c & 31) << 6) | ((unsigned char)s[i++] & 63);
  else if (c >= 0xE0 && c <= 0xEF && i + 1 < s.size()) { cp = ((c & 15) << 12) | (((unsigned char)s[i] & 63) << 6) | ((unsigned char)s[i + 1] & 63); i += 2; }
  else if (c >= 0x80) { while (i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) ++i; cp = '?'; }
  int index = cp >= 32 && cp < 256 ? cp - 32 : -1;
  for (int k = 0; index < 0 && k < extraCount; ++k) if (extraCodepoints[k] == cp) index = 224 + k;
  out.push_back(index < 0 ? '?' - 32 : index);
 }
 return out;
}

int Painter::measure(const std::string& s, int size, int weight) {
 Font* f = font(weight, size);
 if (!f) return int(s.size() * size * .55f);
 auto* chars = static_cast<stbtt_packedchar*>(f->chars);
 float w = 0;
 for (int g : decode(s)) w += chars[g].xadvance;
 return int(w + .5f);
}

int Painter::text(const std::string& s, int x, int y, int size, SDL_Color c, int weight, int maxWidth, int align) {
 Font* f = font(weight, size);
 if (!f || s.empty()) return 0;
 auto* chars = static_cast<stbtt_packedchar*>(f->chars);
 auto glyphs = decode(s);
 float width = 0;
 for (int g : glyphs) width += chars[g].xadvance;
 if (width > maxWidth) {
  float dots = chars['.' - 32].xadvance * 3;
  while (!glyphs.empty() && width + dots > maxWidth) { width -= chars[glyphs.back()].xadvance; glyphs.pop_back(); }
  while (!glyphs.empty() && glyphs.back() == 0) { width -= chars[0].xadvance; glyphs.pop_back(); }
  glyphs.insert(glyphs.end(), 3, '.' - 32); width += dots;
 }
 float startX = align == AlignCenter ? x + (maxWidth - width) / 2.f : align == AlignRight ? x + maxWidth - width : float(x);
 float penX = startX, penY = y + size * .80f;
 SDL_SetTextureColorMod(f->atlas, c.r, c.g, c.b);
 SDL_SetTextureAlphaMod(f->atlas, c.a);
 for (int g : glyphs) {
  stbtt_aligned_quad q;
  stbtt_GetPackedQuad(chars, f->atlasSize, f->atlasSize, g, &penX, &penY, &q, 0);
  SDL_Rect src{int(q.s0 * f->atlasSize + .5f), int(q.t0 * f->atlasSize + .5f), int((q.s1 - q.s0) * f->atlasSize + .5f), int((q.t1 - q.t0) * f->atlasSize + .5f)};
  SDL_FRect dst{q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0};
  if (src.w > 0 && src.h > 0) SDL_RenderCopyF(r, f->atlas, &src, &dst);
 }
 return int(width + .5f);
}

int Painter::paragraph(const std::string& s, int x, int y, int size, SDL_Color c, int width, int lineHeight, int maxLines, int weight) {
 std::vector<std::string> words;
 std::string word;
 for (char ch : s) { if (ch == ' ') { if (!word.empty()) words.push_back(word); word.clear(); } else word += ch; }
 if (!word.empty()) words.push_back(word);
 std::string line;
 int lines = 0;
 for (size_t i = 0; i < words.size(); ++i) {
  std::string next = line.empty() ? words[i] : line + " " + words[i];
  if (!line.empty() && measure(next, size, weight) > width) {
   if (lines + 1 == maxLines) { text(line + " " + words[i] + "...", x, y + lines * lineHeight, size, c, weight, width); return lines + 1; }
   text(line, x, y + lines * lineHeight, size, c, weight, width);
   ++lines; line = words[i];
  } else line = next;
 }
 if (!line.empty()) { text(line, x, y + lines * lineHeight, size, c, weight, width); ++lines; }
 return lines;
}

void Painter::fill(int x, int y, int w, int h, SDL_Color c) {
 if (w <= 0 || h <= 0) return;
 SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
 SDL_Rect a{x, y, w, h};
 SDL_RenderFillRect(r, &a);
}

void Painter::quarter(int x, int y, int rad, int corner, SDL_Color c) {
 if (!disc) return;
 SDL_Rect src{(corner == 1 || corner == 3) ? 64 : 0, corner >= 2 ? 64 : 0, 64, 64};
 SDL_Rect dst{x, y, rad, rad};
 SDL_SetTextureColorMod(disc, c.r, c.g, c.b);
 SDL_SetTextureAlphaMod(disc, c.a);
 SDL_RenderCopy(r, disc, &src, &dst);
}

void Painter::round(int x, int y, int w, int h, int rad, SDL_Color c) {
 if (w <= 0 || h <= 0 || c.a == 0) return;
 rad = std::min({rad, w / 2, h / 2});
 if (rad <= 1 || !disc) { fill(x, y, w, h, c); return; }
 fill(x + rad, y, w - 2 * rad, h, c);
 fill(x, y + rad, rad, h - 2 * rad, c);
 fill(x + w - rad, y + rad, rad, h - 2 * rad, c);
 quarter(x, y, rad, 0, c);
 quarter(x + w - rad, y, rad, 1, c);
 quarter(x, y + h - rad, rad, 2, c);
 quarter(x + w - rad, y + h - rad, rad, 3, c);
}

void Painter::circle(int cx, int cy, int rad, SDL_Color c) { if (rad > 0) round(cx - rad, cy - rad, rad * 2, rad * 2, rad, c); }
void Painter::ring(int cx, int cy, int rad, int t, SDL_Color c, SDL_Color inside) { circle(cx, cy, rad, c); circle(cx, cy, rad - t, inside); }

void Painter::shadow(int x, int y, int w, int h, int rad, int strength) {
 Uint8 base = theme.dark ? 40 : 14;
 for (int i = 3; i >= 1; --i) {
  int grow = i * 2;
  round(x - grow / 2, y + i * 2 - grow / 2 + 1, w + grow, h + grow, rad + grow / 2, {0, 0, 0, Uint8(std::min(255, base * strength / i))});
 }
}

SDL_Color Painter::cursorColor() const {
 float t = .5f + .5f * std::sin(SDL_GetTicks() * 6.2831853f / 1100.f);
 return mix(theme.acc, theme.surf, .55f * t);
}

void Painter::cursor(int x, int y, int w, int h, int rad, SDL_Color gap) {
 round(x - 7, y - 7, w + 14, h + 14, rad + 7, cursorColor());
 round(x - 3, y - 3, w + 6, h + 6, rad + 3, gap);
}

void Painter::maskCorners(int x, int y, int w, int h, int rad, SDL_Color under) {
 rad = std::min({rad, w / 2, h / 2});
 if (rad <= 1 || !inverse) return;
 SDL_SetTextureColorMod(inverse, under.r, under.g, under.b);
 SDL_SetTextureAlphaMod(inverse, under.a);
 const SDL_Rect corners[4] = {{x, y, rad, rad}, {x + w - rad, y, rad, rad}, {x, y + h - rad, rad, rad}, {x + w - rad, y + h - rad, rad, rad}};
 const SDL_RendererFlip flips[4] = {SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL, SDL_RendererFlip(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL)};
 for (int i = 0; i < 4; ++i) SDL_RenderCopyEx(r, inverse, nullptr, &corners[i], 0, nullptr, flips[i]);
}

void Painter::picture(SDL_Texture* t, int tw, int th, int x, int y, int w, int h, int rad, SDL_Color under) {
 if (!t || tw <= 0 || th <= 0 || w <= 0 || h <= 0) return;
 float scale = std::max(float(w) / tw, float(h) / th);
 float sw = w / scale, sh = h / scale;
 SDL_Rect src{int((tw - sw) / 2), int((th - sh) / 2), std::max(1, int(sw)), std::max(1, int(sh))};
 SDL_Rect dst{x, y, w, h};
 SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
 SDL_RenderCopy(r, t, &src, &dst);
 maskCorners(x, y, w, h, rad, under);
}

void Painter::gradient(int x, int y, int w, int h, SDL_Color top, SDL_Color bottom) {
 SDL_Vertex v[4];
 SDL_FPoint p[4] = {{float(x), float(y)}, {float(x + w), float(y)}, {float(x + w), float(y + h)}, {float(x), float(y + h)}};
 SDL_Color c[4] = {top, top, bottom, bottom};
 for (int i = 0; i < 4; ++i) { v[i].position = p[i]; v[i].color = c[i]; v[i].tex_coord = {0, 0}; }
 int idx[6] = {0, 1, 2, 0, 2, 3};
 SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

int Painter::button(const std::string& label, int x, int cy, SDL_Color fillColor, SDL_Color ink) {
 int w = label.size() > 1 ? std::max(30, measure(label, 14, Heavy) + 16) : 30;
 round(x, cy - 15, w, 30, 15, fillColor);
 text(label, x, cy - 9, 14, ink, Heavy, w, AlignCenter);
 return w;
}

// ------------------------------------------------------------------ icons
namespace {
struct Shape {
 enum Kind { Seg, Arc, Disc, Ring, Poly } kind;
 float x1 = 0, y1 = 0, x2 = 0, y2 = 0, rad = 0, a0 = 0, a1 = 0;
 std::vector<SDL_FPoint> pts;
 bool closed = false, filled = false;
};
struct IconDef {
 std::vector<Shape> shapes;
 IconDef& seg(float a, float b, float c, float d) { Shape s{Shape::Seg}; s.x1 = a; s.y1 = b; s.x2 = c; s.y2 = d; shapes.push_back(s); return *this; }
 IconDef& line(std::vector<SDL_FPoint> p, bool closed = false, bool filled = false) { Shape s{Shape::Poly}; s.pts = std::move(p); s.closed = closed || filled; s.filled = filled; shapes.push_back(s); return *this; }
 IconDef& arc(float cx, float cy, float r, float from, float to) { Shape s{Shape::Arc}; s.x1 = cx; s.y1 = cy; s.rad = r; s.a0 = from; s.a1 = to; shapes.push_back(s); return *this; }
 IconDef& dot(float cx, float cy, float r) { Shape s{Shape::Disc}; s.x1 = cx; s.y1 = cy; s.rad = r; shapes.push_back(s); return *this; }
 IconDef& ring(float cx, float cy, float r) { Shape s{Shape::Ring}; s.x1 = cx; s.y1 = cy; s.rad = r; shapes.push_back(s); return *this; }
 IconDef& rect(float x0, float y0, float x1, float y1) { return line({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, true); }
};
float segDistance(float px, float py, float ax, float ay, float bx, float by) {
 float dx = bx - ax, dy = by - ay, l = dx * dx + dy * dy;
 float t = l > 0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / l, 0.f, 1.f) : 0.f;
 float x = ax + t * dx - px, y = ay + t * dy - py;
 return std::sqrt(x * x + y * y);
}
bool insideShape(const Shape& s, float x, float y, float hw) {
 switch (s.kind) {
 case Shape::Seg: return segDistance(x, y, s.x1, s.y1, s.x2, s.y2) <= hw;
 case Shape::Disc: return (x - s.x1) * (x - s.x1) + (y - s.y1) * (y - s.y1) <= s.rad * s.rad;
 case Shape::Ring: { float d = std::sqrt((x - s.x1) * (x - s.x1) + (y - s.y1) * (y - s.y1)); return std::fabs(d - s.rad) <= hw; }
 case Shape::Arc: {
  float dx = x - s.x1, dy = y - s.y1, d = std::sqrt(dx * dx + dy * dy);
  const float k = 3.14159265f / 180.f;
  for (float a : {s.a0, s.a1}) { float ex = s.x1 + s.rad * std::cos(a * k), ey = s.y1 + s.rad * std::sin(a * k); if ((x - ex) * (x - ex) + (y - ey) * (y - ey) <= hw * hw) return true; }
  if (std::fabs(d - s.rad) > hw) return false;
  float ang = std::atan2(dy, dx) / k;
  while (ang < s.a0) ang += 360.f;
  while (ang > s.a0 + 360.f) ang -= 360.f;
  return ang <= s.a1;
 }
 case Shape::Poly: {
  size_t n = s.pts.size();
  for (size_t i = 0; i + 1 < n + (s.closed ? 1 : 0); ++i) {
   auto& a = s.pts[i]; auto& b = s.pts[(i + 1) % n];
   if (segDistance(x, y, a.x, a.y, b.x, b.y) <= hw) return true;
  }
  if (!s.filled) return false;
  bool in = false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
   auto& a = s.pts[i]; auto& b = s.pts[j];
   if (((a.y > y) != (b.y > y)) && (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)) in = !in;
  }
  return in;
 }
 }
 return false;
}
IconDef define(Icon id) {
 IconDef d;
 switch (id) {
 case Icon::Play: d.line({{8.5f, 6}, {18, 12}, {8.5f, 18}}, true, true); break;
 case Icon::Save: d.seg(12, 4, 12, 15).seg(7.5f, 10.5f, 12, 15).seg(16.5f, 10.5f, 12, 15).seg(5, 19.5f, 19, 19.5f); break;
 case Icon::Load: d.seg(12, 15, 12, 4).seg(7.5f, 8.5f, 12, 4).seg(16.5f, 8.5f, 12, 4).seg(5, 19.5f, 19, 19.5f); break;
 case Icon::Rewind: d.line({{11, 7.5f}, {5.5f, 12}, {11, 16.5f}}, true, true).line({{19, 7.5f}, {13.5f, 12}, {19, 16.5f}}, true, true); break;
 case Icon::FastForward: d.line({{5, 7.5f}, {10.5f, 12}, {5, 16.5f}}, true, true).line({{13, 7.5f}, {18.5f, 12}, {13, 16.5f}}, true, true); break;
 case Icon::Cheat: d.line({{11, 3.5f}, {12.8f, 8.3f}, {17.6f, 10.1f}, {12.8f, 11.9f}, {11, 16.7f}, {9.2f, 11.9f}, {4.4f, 10.1f}, {9.2f, 8.3f}}, true).seg(18, 15, 18, 20.5f).seg(15.5f, 17.75f, 20.5f, 17.75f); break;
 case Icon::Display: d.rect(3.5f, 5, 20.5f, 16.5f).seg(9, 20, 15, 20).seg(12, 16.5f, 12, 20); break;
 case Icon::Pad: d.line({{7.2f, 8}, {16.8f, 8}, {19.4f, 9.4f}, {20.7f, 13}, {20.1f, 16.4f}, {18.6f, 17.9f}, {16.6f, 17.4f}, {14.5f, 15.5f}, {9.5f, 15.5f}, {7.4f, 17.4f}, {5.4f, 17.9f}, {3.9f, 16.4f}, {3.3f, 13}, {4.6f, 9.4f}}, true).seg(8, 10.6f, 8, 14.2f).seg(6.2f, 12.4f, 9.8f, 12.4f).dot(15.6f, 11.2f, 1.15f).dot(17.4f, 13.4f, 1.15f); break;
 case Icon::Camera: d.rect(3.5f, 7.5f, 20.5f, 19).line({{8.8f, 7.5f}, {10.2f, 4.8f}, {13.8f, 4.8f}, {15.2f, 7.5f}}).ring(12, 13.2f, 3.3f); break;
 case Icon::Restart: d.arc(12, 12, 7.3f, -135, 180).line({{4.7f, 4.4f}, {4.7f, 8.2f}, {8.5f, 8.2f}}); break;
 case Icon::Update: d.arc(12, 12, 7.3f, 0, 315).line({{19.3f, 4.4f}, {19.3f, 8.2f}, {15.5f, 8.2f}}); break;
 case Icon::Power: d.arc(12, 12.8f, 7, -51, 231).seg(12, 3.5f, 12, 11.5f); break;
 case Icon::Search: d.ring(10.8f, 10.8f, 6.2f).seg(15.4f, 15.4f, 19.8f, 19.8f); break;
 case Icon::Star: d.line({{12, 3.6f}, {14.6f, 8.8f}, {20.2f, 9.6f}, {16.1f, 13.5f}, {17.1f, 19.2f}, {12, 16.5f}, {6.9f, 19.2f}, {7.9f, 13.5f}, {3.8f, 9.6f}, {9.4f, 8.8f}}, true, true); break;
 case Icon::StarOutline: d.line({{12, 3.6f}, {14.6f, 8.8f}, {20.2f, 9.6f}, {16.1f, 13.5f}, {17.1f, 19.2f}, {12, 16.5f}, {6.9f, 19.2f}, {7.9f, 13.5f}, {3.8f, 9.6f}, {9.4f, 8.8f}}, true); break;
 case Icon::Settings: d.seg(4.5f, 7.5f, 13.3f, 7.5f).seg(18.3f, 7.5f, 19.5f, 7.5f).seg(4.5f, 16.5f, 5.7f, 16.5f).seg(10.7f, 16.5f, 19.5f, 16.5f).ring(15.8f, 7.5f, 2.3f).ring(8.2f, 16.5f, 2.3f); break;
 case Icon::Grid: d.rect(4, 4, 10.8f, 10.8f).rect(13.2f, 4, 20, 10.8f).rect(4, 13.2f, 10.8f, 20).rect(13.2f, 13.2f, 20, 20); break;
 case Icon::Home: d.line({{4.5f, 11}, {12, 4.5f}, {19.5f, 11}, {19.5f, 20}, {14.5f, 20}, {14.5f, 15}, {9.5f, 15}, {9.5f, 20}, {4.5f, 20}}, true); break;
 case Icon::Palette: d.ring(12, 12, 8.4f).dot(8, 12.6f, 1.3f).dot(9.6f, 8.4f, 1.3f).dot(14, 7.6f, 1.3f).dot(16.4f, 11, 1.3f).dot(14.2f, 15.6f, 1.6f); break;
 case Icon::Map: d.line({{9, 5}, {4, 7}, {4, 19}, {9, 17}, {15, 19}, {20, 17}, {20, 5}, {15, 7}}, true).seg(9, 5, 9, 17).seg(15, 7, 15, 19); break;
 case Icon::Info: d.ring(12, 12, 8.5f).seg(12, 11, 12, 16).dot(12, 7.8f, 1.3f); break;
 case Icon::Plus: d.seg(12, 5.5f, 12, 18.5f).seg(5.5f, 12, 18.5f, 12); break;
 case Icon::Volume: d.line({{4.5f, 9.5f}, {7.8f, 9.5f}, {12, 5.8f}, {12, 18.2f}, {7.8f, 14.5f}, {4.5f, 14.5f}}, true).arc(12.5f, 12, 4, -45, 45).arc(12.5f, 12, 7.5f, -50, 50); break;
 case Icon::Wifi: d.arc(12, 19, 12, -130, -50).arc(12, 19, 7.8f, -130, -50).arc(12, 19, 3.6f, -130, -50).dot(12, 19, 1.4f); break;
 case Icon::Clock: d.ring(12, 12, 8).line({{12, 7.5f}, {12, 12}, {15, 14}}); break;
 case Icon::Check: d.line({{5.5f, 12.5f}, {9.5f, 16.5f}, {18.5f, 7.5f}}); break;
 case Icon::Folder: d.line({{3.5f, 6}, {9.5f, 6}, {11.5f, 8}, {20.5f, 8}, {20.5f, 19}, {3.5f, 19}}, true); break;
 case Icon::Trash: d.seg(4.5f, 7, 19.5f, 7).seg(10, 4.5f, 14, 4.5f).line({{6.5f, 7}, {17.5f, 7}, {16.5f, 20}, {7.5f, 20}}, true).seg(10.5f, 10.5f, 10.5f, 16.5f).seg(13.5f, 10.5f, 13.5f, 16.5f); break;
 case Icon::Left: d.line({{14.5f, 5.5f}, {8, 12}, {14.5f, 18.5f}}); break;
 case Icon::Right: d.line({{9.5f, 5.5f}, {16, 12}, {9.5f, 18.5f}}); break;
 case Icon::Up: d.line({{5.5f, 14.5f}, {12, 8}, {18.5f, 14.5f}}); break;
 case Icon::Down: d.line({{5.5f, 9.5f}, {12, 16}, {18.5f, 9.5f}}); break;
 case Icon::Undo: d.line({{8.5f, 5}, {4.5f, 9}, {8.5f, 13}}).seg(4.5f, 9, 14, 9).arc(14, 14, 5, -90, 90).seg(14, 19, 8, 19); break;
 case Icon::Keyboard: d.rect(3, 6.5f, 21, 17.5f).dot(7, 10, 1).dot(10.3f, 10, 1).dot(13.6f, 10, 1).dot(17, 10, 1).seg(8, 14, 16, 14); break;
 case Icon::Sort: d.seg(5, 7, 19, 7).seg(5, 12, 15, 12).seg(5, 17, 11, 17); break;
 default: break;
 }
 return d;
}
} // namespace

SDL_Texture* Painter::iconTexture(Icon id, int size) {
 long key = long(id) * 1000 + size;
 auto it = icons.find(key);
 if (it != icons.end()) return it->second;
 IconDef def = define(id);
 std::vector<float> a(size_t(size) * size, 0.f);
 float unit = 24.f / size, hw = 1.05f;
 for (int py = 0; py < size; ++py)
  for (int px = 0; px < size; ++px) {
   int hit = 0;
   for (int sy = 0; sy < 4; ++sy)
    for (int sx = 0; sx < 4; ++sx) {
     float x = (px + (sx + .5f) / 4.f) * unit, y = (py + (sy + .5f) / 4.f) * unit;
     for (auto& s : def.shapes) if (insideShape(s, x, y, hw)) { ++hit; break; }
    }
   a[size_t(py) * size + px] = hit / 16.f;
  }
 SDL_Texture* t = alphaTexture(r, size, size, a);
 icons[key] = t;
 return t;
}

void Painter::icon(Icon id, int cx, int cy, int size, SDL_Color c) {
 if (!r || size <= 0) return;
 SDL_Texture* t = iconTexture(id, size);
 if (!t) return;
 SDL_SetTextureColorMod(t, c.r, c.g, c.b);
 SDL_SetTextureAlphaMod(t, c.a);
 SDL_Rect dst{cx - size / 2, cy - size / 2, size, size};
 SDL_RenderCopy(r, t, nullptr, &dst);
}

} // namespace ui
