// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// v0.5 drawing kit shared by the library and the in-game menu: theme tokens,
// anti-aliased rounded shapes, vector icons and TrueType text on any SDL_Renderer
// (hardware or software), so the N64 runner can draw its menu off-screen too.
#pragma once
#include <SDL.h>
#include <map>
#include <string>
#include <vector>

// Controller buttons as plain bits, so the UI code does not depend on libnx.
namespace pad {
enum : uint32_t {
 A = 1u << 0, B = 1u << 1, X = 1u << 2, Y = 1u << 3, L = 1u << 4, R = 1u << 5, ZL = 1u << 6, ZR = 1u << 7,
 Plus = 1u << 8, Minus = 1u << 9, Up = 1u << 10, Down = 1u << 11, Left = 1u << 12, Right = 1u << 13,
 LStick = 1u << 14, RStick = 1u << 15
};
}

namespace ui {

struct Theme {
 std::string id, name;
 SDL_Color bg, surf, surf2, line, text, muted, acc, accText, btn, btnText;
 bool dark = false;
};
const std::vector<Theme>& presetThemes();
const Theme& presetTheme(const std::string& id);

SDL_Color rgba(Uint32 rgb, Uint8 a = 255);
SDL_Color mix(SDL_Color a, SDL_Color b, float t);
SDL_Color alpha(SDL_Color c, Uint8 a);
bool parseHex(const std::string& hex, SDL_Color& out);
std::string toHex(SDL_Color c);
// Readable text colour (dark or white) on top of the given fill.
SDL_Color onColor(SDL_Color fill);

enum Weight { Medium = 0, Bold = 1, Heavy = 2, Mono = 3 };
enum FontFamily { FontRounded = 0, FontRubik = 1, FontNunito = 2, FontFamilyCount = 3 };
const char* fontFamilyName(int family);

enum class Icon {
 Play, Save, Load, Rewind, FastForward, Cheat, Display, Pad, Camera, Restart, Power, Search, Star, StarOutline,
 Settings, Grid, Home, Palette, Update, Map, Info, Plus, Volume, Wifi, Clock, Check, Folder, Trash,
 Left, Right, Up, Down, Undo, Keyboard, Sort, Count
};

enum Align { AlignLeft = 0, AlignCenter = 1, AlignRight = 2 };

class Painter {
public:
 explicit Painter(SDL_Renderer* renderer);
 ~Painter();
 Painter(const Painter&) = delete;
 Painter& operator=(const Painter&) = delete;

 SDL_Renderer* r;
 Theme theme;
 int family = FontRounded;
 int corners = 1; // 0 square, 1 soft, 2 round: scales every radius

 void setTheme(const Theme& t) { theme = t; }
 int radius(int base) const;

 // Text: y is the top of the line box. Returns the drawn width.
 int text(const std::string& s, int x, int y, int size, SDL_Color c, int weight = Medium, int maxWidth = 4000, int align = AlignLeft);
 int measure(const std::string& s, int size, int weight = Medium);
 // Wraps to at most maxLines lines; returns the number of lines drawn.
 int paragraph(const std::string& s, int x, int y, int size, SDL_Color c, int width, int lineHeight, int maxLines = 8, int weight = Medium);

 void fill(int x, int y, int w, int h, SDL_Color c);
 void round(int x, int y, int w, int h, int radius, SDL_Color c);
 void circle(int cx, int cy, int radius, SDL_Color c);
 void ring(int cx, int cy, int radius, int thickness, SDL_Color c, SDL_Color inside);
 void shadow(int x, int y, int w, int h, int radius, int strength = 1);
 // Pulsing selection outline drawn around a box; draw the box itself afterwards.
 void cursor(int x, int y, int w, int h, int radius, SDL_Color gap);
 SDL_Color cursorColor() const;
 void icon(Icon id, int cx, int cy, int size, SDL_Color c);
 // Controller button glyph (round or pill); returns its width.
 int button(const std::string& label, int x, int cy, SDL_Color fill, SDL_Color ink);
 // Centre-cropped picture with anti-aliased rounded corners blended against `under`.
 void picture(SDL_Texture* t, int tw, int th, int x, int y, int w, int h, int radius, SDL_Color under);
 void maskCorners(int x, int y, int w, int h, int radius, SDL_Color under);

 void gradient(int x, int y, int w, int h, SDL_Color top, SDL_Color bottom);

private:
 struct Font { SDL_Texture* atlas = nullptr; void* chars = nullptr; int atlasSize = 0; int size = 0; };
 std::vector<unsigned char> fontData[FontFamilyCount][4];
 bool fontLoaded[FontFamilyCount][4] = {};
 std::vector<unsigned char> fallback;
 std::map<long, Font> fonts;
 std::map<long, SDL_Texture*> icons;
 SDL_Texture* disc = nullptr;     // anti-aliased filled circle, 128 px
 SDL_Texture* inverse = nullptr;  // anti-aliased outside-corner mask, 64 px
 const std::vector<unsigned char>& data(int fam, int weight);
 Font* font(int weight, int size);
 SDL_Texture* iconTexture(Icon id, int size);
 void quarter(int x, int y, int r, int corner, SDL_Color c);
};

// Shared helpers for game screens (library cards and the in-game menu).
std::string initials(const std::string& title);
// "Legend of Zelda, The - The Minish Cap (USA)" -> "The Legend of Zelda - The Minish Cap"
std::string prettyTitle(const std::string& title);
SDL_Color consoleColor(int platform);

} // namespace ui
