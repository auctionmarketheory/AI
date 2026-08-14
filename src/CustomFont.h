#ifndef CUSTOMFONT_H
#define CUSTOMFONT_H

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "stb_truetype.h"

struct RGBA {
    Uint8 r, g, b, a;
};

class CustomFont {
public:
    stbtt_fontinfo info;
    std::vector<unsigned char> ttf_buffer;
    float scale;
    int ascent, descent, lineGap;
    float sz = 20.0f;
    
    struct Glyph {
        SDL_Texture* tex;
        int w, h, xoff, yoff, advance;
    };
    std::unordered_map<int, Glyph> cache;

    bool load(SDL_Renderer* r, const std::string& path, float s);
    Glyph getGlyph(SDL_Renderer* r, int cp);
    uint32_t decodeUTF8(const std::string& str, size_t& i);
    void draw(SDL_Renderer* r, float x, float y, const std::string& txt, RGBA c);
    int getTextWidth(SDL_Renderer* r, const std::string& txt);
    void freeCache();
};

#endif
