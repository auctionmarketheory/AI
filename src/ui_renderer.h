#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <SDL2/SDL.h>
#include <string>
#include "CustomFont.h"

// Biểu cảm của AI
enum class FaceExpression {
    IDLE,
    THINKING,
    TALKING,
    ERROR
};

class UiRenderer {
public:
    UiRenderer(SDL_Renderer* renderer, CustomFont* font);
    ~UiRenderer();

    void setExpression(FaceExpression expr);
    void setUserMessage(const std::string& msg, bool showCursor);
    void setAiMessage(const std::string& msg);
    
    // Gọi mỗi frame để vẽ
    void render(int screenWidth, int screenHeight);

    // Hiệu ứng chớp mắt & gõ chữ
    void update(float deltaTime);

private:
    SDL_Renderer* renderer;
    CustomFont* font;

    FaceExpression currentExpression = FaceExpression::IDLE;
    std::string userMessage = "";
    std::string aiMessage = "Xin chao, toi la AMT Assist!";
    bool showCursor = false;
    float cursorTimer = 0.0f;
    
    // Typewriter effect cho AI
    float typewriterTimer = 0.0f;
    size_t charactersToShow = 0;

    // Blink effect
    float blinkTimer = 0.0f;
    bool isBlinking = false;

    void drawFace(int cx, int cy);
    void drawTextBoxes(int screenWidth, int screenHeight);
    
    // Hàm phụ trợ vẽ hcn
    void fillRect(int x, int y, int w, int h, RGBA color);
    void drawRect(int x, int y, int w, int h, RGBA color);
};

#endif
