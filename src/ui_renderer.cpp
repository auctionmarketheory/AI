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
    int eyeWidth = 40;
    int eyeHeight = isBlinking ? 10 : 50; // Khi chớp mắt, mắt híp lại
    int eyeSpacing = 80;
    int eyeY = cy - 40;

    if (currentExpression == FaceExpression::THINKING) {
        // Mắt nhấp nháy, đang suy nghĩ
        eyeWidth = 30;
        eyeHeight = 30;
    } else if (currentExpression == FaceExpression::ERROR) {
        // Mắt chéo
        eyeWidth = 40;
        eyeHeight = 10;
    }

    RGBA eyeColor = (currentExpression == FaceExpression::ERROR) ? COLOR_PINK : COLOR_CYAN;

    // Vẽ mắt trái
    fillRect(cx - eyeSpacing/2 - eyeWidth, eyeY, eyeWidth, eyeHeight, eyeColor);
    // Vẽ mắt phải
    fillRect(cx + eyeSpacing/2, eyeY, eyeWidth, eyeHeight, eyeColor);

    // Vẽ miệng
    int mouthWidth = 60;
    int mouthHeight = 15;
    int mouthY = cy + 40;

    if (currentExpression == FaceExpression::TALKING) {
        // Miệng to ra khi nói
        mouthHeight = 30;
        mouthWidth = 70;
    } else if (currentExpression == FaceExpression::ERROR) {
        // Miệng mếu
        mouthHeight = 10;
        mouthY += 10;
    } else if (currentExpression == FaceExpression::THINKING) {
        // Miệng nhỏ
        mouthWidth = 30;
    }

    fillRect(cx - mouthWidth/2, mouthY, mouthWidth, mouthHeight, eyeColor);
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
    int boxX = 20;
    int boxW = screenWidth - 40;
    
    // Khung 1: YOU (User Input)
    int uBoxH = 60;
    int uBoxY = screenHeight - 200; // Nâng lên cao hơn
    fillRect(boxX, uBoxY, boxW, uBoxH, COLOR_DIM);
    drawRect(boxX, uBoxY, boxW, uBoxH, COLOR_CYAN);
    
    std::string displayUserStr = "YOU: " + userMessage;
    if (showCursor && cursorTimer < 0.5f) displayUserStr += "_";
    drawWrappedText(renderer, font, displayUserStr, boxX + 15, uBoxY + 15, boxW - 30, COLOR_CYAN);

    // Khung 2: AMT ASSIST (AI Response)
    int aBoxH = 110;
    int aBoxY = screenHeight - 130;
    fillRect(boxX, aBoxY, boxW, aBoxH, COLOR_DIM);
    drawRect(boxX, aBoxY, boxW, aBoxH, COLOR_PINK);
    drawRect(boxX-1, aBoxY-1, boxW+2, aBoxH+2, COLOR_PINK);

    std::string textToDraw = "AMT: " + aiMessage.substr(0, charactersToShow);
    drawWrappedText(renderer, font, textToDraw, boxX + 15, aBoxY + 15, boxW - 30, COLOR_WHITE);
}

void UiRenderer::render(int screenWidth, int screenHeight) {
    // Vẽ background
    fillRect(0, 0, screenWidth, screenHeight, COLOR_BG);

    // Vẽ Face ở giữa phía trên (nâng cao lên một chút)
    drawFace(screenWidth / 2, screenHeight / 2 - 90);

    // Vẽ 2 Khung Chat
    drawTextBoxes(screenWidth, screenHeight);
}
