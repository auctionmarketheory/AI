#include "ui_renderer.h"

// Màu Cyberpunk
const RGBA COLOR_BG = {10, 10, 20, 255};
const RGBA COLOR_CYAN = {0, 255, 255, 255};
const RGBA COLOR_PINK = {255, 0, 127, 255};
const RGBA COLOR_WHITE = {255, 255, 255, 255};
const RGBA COLOR_DIM = {50, 50, 60, 255};

UiRenderer::UiRenderer(SDL_Renderer* renderer, CustomFont* font) 
    : renderer(renderer), font(font) {
}

UiRenderer::~UiRenderer() {
}

void UiRenderer::setExpression(FaceExpression expr) {
    currentExpression = expr;
    // Reset blink
    isBlinking = false;
    blinkTimer = 0.0f;
}

void UiRenderer::setUserMessage(const std::string& msg, bool cursor) {
    userMessage = msg;
    showCursor = cursor;
}

void UiRenderer::setAiMessage(const std::string& msg) {
    aiMessage = msg;
    charactersToShow = 0;
    typewriterTimer = 0.0f;
}

void UiRenderer::update(float deltaTime) {
    // Logic chớp mắt (Blink)
    blinkTimer += deltaTime;
    if (isBlinking) {
        if (blinkTimer > 0.15f) { // Nhắm mắt trong 0.15s
            isBlinking = false;
            blinkTimer = 0.0f;
        }
    } else {
        if (blinkTimer > 3.0f) { // Mở mắt 3s rồi chớp
            isBlinking = true;
            blinkTimer = 0.0f;
        }
    }

    // Nháy con trỏ
    cursorTimer += deltaTime;
    if (cursorTimer > 1.0f) cursorTimer = 0.0f;

    // Logic gõ phím (Typewriter) cho AI
    if (charactersToShow < aiMessage.length()) {
        typewriterTimer += deltaTime;
        if (typewriterTimer > 0.02f) { // Tốc độ gõ 0.02s / ký tự
            charactersToShow++;
            typewriterTimer = 0.0f;
            
            // Fix utf8 split (không cắt ngang ký tự utf-8)
            while (charactersToShow < aiMessage.length() && 
                  (aiMessage[charactersToShow] & 0xC0) == 0x80) {
                charactersToShow++;
            }
        }
    }
}

void UiRenderer::fillRect(int x, int y, int w, int h, RGBA color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void UiRenderer::drawRect(int x, int y, int w, int h, RGBA color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderDrawRect(renderer, &r);
}

void UiRenderer::drawFace(int cx, int cy) {
    std::string l1 = "   /\\_/\\   ";
    std::string l2 = "  ( o.o )  ";
    std::string l3 = "   > ^ <   ";
    
    if (currentExpression == FaceExpression::THINKING) {
        l2 = "  ( -.- )  ";
    } else if (currentExpression == FaceExpression::ERROR) {
        l2 = "  ( x.x )  ";
    } else if (currentExpression == FaceExpression::TALKING) {
        l2 = "  ( o.O )  ";
    } else if (isBlinking) {
        l2 = "  ( -.- )  ";
    }
    
    RGBA color = (currentExpression == FaceExpression::ERROR) ? COLOR_PINK : COLOR_CYAN;
    
    int w1 = font->getTextWidth(renderer, l1);
    font->draw(renderer, cx - w1/2, cy - 40, l1, color);
    
    int w2 = font->getTextWidth(renderer, l2);
    font->draw(renderer, cx - w2/2, cy, l2, color);
    
    int w3 = font->getTextWidth(renderer, l3);
    font->draw(renderer, cx - w3/2, cy + 40, l3, color);
}

void drawWrappedText(SDL_Renderer* renderer, CustomFont* font, std::string text, int x, int y, int maxW, RGBA color) {
    std::string currentLine = "";
    int textX = x;
    int textY = y;
    int lineHeight = font->sz + 10;
    size_t i = 0;
    while (i < text.length()) {
        size_t nextSpace = text.find(' ', i);
        if (nextSpace == std::string::npos) nextSpace = text.length();
        
        std::string word = text.substr(i, nextSpace - i + 1);
        int lineWidth = font->getTextWidth(renderer, currentLine + word);
        if (lineWidth > maxW && currentLine.length() > 0) {
            font->draw(renderer, textX, textY, currentLine, color);
            textY += lineHeight;
            currentLine = word;
        } else {
            currentLine += word;
        }
        i = nextSpace + 1;
    }
    if (currentLine.length() > 0) {
        font->draw(renderer, textX, textY, currentLine, color);
    }
}

void UiRenderer::drawTextBoxes(int screenWidth, int screenHeight) {
    int boxX = 10;
    int boxW = screenWidth - 20;
    
    // Khung 1: YOU (User Input)
    int uBoxH = 60;
    int uBoxY = screenHeight - 205;
    fillRect(boxX, uBoxY, boxW, uBoxH, COLOR_DIM);
    drawRect(boxX, uBoxY, boxW, uBoxH, COLOR_CYAN);
    
    std::string displayUserStr = "YOU: " + userMessage;
    if (showCursor && cursorTimer < 0.5f) displayUserStr += "_";
    drawWrappedText(renderer, font, displayUserStr, boxX + 10, uBoxY + 15, boxW - 20, COLOR_CYAN);

    // Khung 2: AMT ASSIST (AI Response)
    int aBoxH = 110;
    int aBoxY = screenHeight - 140;
    fillRect(boxX, aBoxY, boxW, aBoxH, COLOR_DIM);
    drawRect(boxX, aBoxY, boxW, aBoxH, COLOR_PINK);
    drawRect(boxX-1, aBoxY-1, boxW+2, aBoxH+2, COLOR_PINK);

    std::string textToDraw = "AMT: " + aiMessage.substr(0, charactersToShow);
    drawWrappedText(renderer, font, textToDraw, boxX + 10, aBoxY + 15, boxW - 20, COLOR_WHITE);
}

void UiRenderer::render(int screenWidth, int screenHeight) {
    // Vẽ background
    fillRect(0, 0, screenWidth, screenHeight, COLOR_BG);

    // Header
    std::string header = "[Online] AMT ASSIST [Gemini]";
    int hw = font->getTextWidth(renderer, header);
    font->draw(renderer, (screenWidth - hw) / 2, 10, header, COLOR_CYAN);
    drawRect(10, 45, screenWidth - 20, 2, COLOR_DIM); // Line

    // Avatar
    drawFace(screenWidth / 2, 120);

    // 2 Khung Chat
    drawTextBoxes(screenWidth, screenHeight);
    
    // Footer
    std::string footer = "[BAN PHIM: Go] [ENTER: Gui] [B/ESC: Thoat]";
    int fw = font->getTextWidth(renderer, footer);
    font->draw(renderer, (screenWidth - fw) / 2, screenHeight - 25, footer, {150, 150, 150, 255});
}
