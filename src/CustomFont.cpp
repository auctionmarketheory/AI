#include "CustomFont.h"
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

bool CustomFont::load(SDL_Renderer* r, const std::string& path, float s) {
    sz = s;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    auto len = f.tellg(); f.seekg(0);
    ttf_buffer.resize(len);
    f.read((char*)ttf_buffer.data(), len);
    
    if (!stbtt_InitFont(&info, ttf_buffer.data(), 0)) return false;
    
    scale = stbtt_ScaleForPixelHeight(&info, s);
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    return true;
}

CustomFont::Glyph CustomFont::getGlyph(SDL_Renderer* r, int cp) {
    if (cache.count(cp)) return cache[cp];
    Glyph g = {nullptr, 0, 0, 0, 0, 0};
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
    g.advance = adv * scale;
    
    int x0,y0,x1,y1;
    stbtt_GetCodepointBitmapBox(&info, cp, scale, scale, &x0, &y0, &x1, &y1);
    g.w = x1 - x0;
    g.h = y1 - y0;
    g.xoff = x0;
    g.yoff = y0;
    
    if (g.w > 0 && g.h > 0) {
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, cp, &g.w, &g.h, &g.xoff, &g.yoff);
        std::vector<unsigned char> rgba(g.w * g.h * 4, 255);
        for (int i=0; i<g.w*g.h; i++) rgba[i*4+3] = bitmap[i];
        stbtt_FreeBitmap(bitmap, nullptr);
        SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(rgba.data(), g.w, g.h, 32, g.w*4, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
        g.tex = SDL_CreateTextureFromSurface(r, sf);
        SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(sf);
    }
    cache[cp] = g;
    return g;
}

uint32_t CustomFont::decodeUTF8(const std::string& str, size_t& i) {
    if (i >= str.length()) return 0;
    unsigned char c0 = str[i];
    if ((c0 & 0x80) == 0) { i += 1; return c0; }
    if ((c0 & 0xE0) == 0xC0) {
        if (i+1 >= str.length()) { i+=1; return 0; }
        unsigned char c1 = str[i+1];
        i += 2; return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
    }
    if ((c0 & 0xF0) == 0xE0) {
        if (i+2 >= str.length()) { i+=1; return 0; }
        unsigned char c1 = str[i+1], c2 = str[i+2];
        i += 3; return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
    }
    if ((c0 & 0xF8) == 0xF0) {
        if (i+3 >= str.length()) { i+=1; return 0; }
        unsigned char c1 = str[i+1], c2 = str[i+2], c3 = str[i+3];
        i += 4; return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    }
    i += 1; return 0;
}

void CustomFont::draw(SDL_Renderer* r, float x, float y, const std::string& txt, RGBA c) {
    float cx = x;
    size_t i = 0;
    int base_y = y + ascent * scale;
    while(i < txt.length()) {
        uint32_t cp = decodeUTF8(txt, i);
        if (!cp) continue;
        Glyph g = getGlyph(r, cp);
        if (g.tex) {
            SDL_SetTextureColorMod(g.tex, c.r, c.g, c.b);
            SDL_SetTextureAlphaMod(g.tex, c.a);
            SDL_Rect d = { (int)(cx + g.xoff), (int)(base_y + g.yoff), g.w, g.h };
            SDL_RenderCopy(r, g.tex, nullptr, &d);
        }
        cx += g.advance;
    }
}

int CustomFont::getTextWidth(SDL_Renderer* r, const std::string& txt) {
    float cx = 0;
    size_t i = 0;
    while(i < txt.length()) {
        uint32_t cp = decodeUTF8(txt, i);
        if (!cp) continue;
        cx += getGlyph(r, cp).advance;
    }
    return (int)cx;
}

void CustomFont::freeCache() {
    for (auto& pair : cache) {
        if (pair.second.tex) SDL_DestroyTexture(pair.second.tex);
    }
    cache.clear();
}
