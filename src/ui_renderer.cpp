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
    int eyeHeight = isBlinking ? 10 : 50;
    int eyeSpacing = 80;
    int eyeY = cy - 40;

    if (currentExpression == FaceExpression::THINKING) {
        eyeWidth = 30; eyeHeight = 30;
    } else if (currentExpression == FaceExpression::ERROR) {
        eyeWidth = 40; eyeHeight = 10;
    }

    RGBA eyeColor = (currentExpression == FaceExpression::ERROR) ? COLOR_PINK : COLOR_CYAN;

    fillRect(cx - eyeSpacing/2 - eyeWidth, eyeY, eyeWidth, eyeHeight, eyeColor);
    fillRect(cx + eyeSpacing/2, eyeY, eyeWidth, eyeHeight, eyeColor);

    int mouthWidth = 60;
    int mouthHeight = 15;
    int mouthY = cy + 40;

    if (currentExpression == FaceExpression::TALKING) {
        mouthHeight = 30; mouthWidth = 70;
    } else if (currentExpression == FaceExpression::ERROR) {
        mouthHeight = 10; mouthY += 10;
    } else if (currentExpression == FaceExpression::THINKING) {
        mouthWidth = 30;
    }
    fillRect(cx - mouthWidth/2, mouthY, mouthWidth, mouthHeight, eyeColor);
}

int drawWrappedText(SDL_Renderer* renderer, CustomFont* font, std::string text, int x, int y, int maxW, RGBA color, bool measureOnly = false) {
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
            if (!measureOnly) font->draw(renderer, textX, textY, currentLine, color);
            textY += lineHeight;
            currentLine = word;
        } else {
            currentLine += word;
        }
        i = nextSpace + 1;
    }
    if (currentLine.length() > 0) {
        if (!measureOnly) font->draw(renderer, textX, textY, currentLine, color);
        textY += lineHeight;
    }
    return textY - y;
}

void UiRenderer::drawTextBoxes(int screenWidth, int screenHeight) {
    int padding = 15;
    int boxW = screenWidth - padding * 2;
    
    // Y offsets
    int yHeader = 40;
    int yFaceBottom = 220;
    int yUser = yFaceBottom;
    int hUser = 70;
    int yAI = yUser + hUser;
    int hAI = 120;
    int yFooter = yAI + hAI;

    // Vẽ các đường kẻ ngang (Terminal ASCII feel)
    drawRect(padding, yHeader, boxW, 1, COLOR_DIM);
    drawRect(padding, yUser, boxW, 1, COLOR_DIM);
    drawRect(padding, yAI, boxW, 1, COLOR_DIM);
    drawRect(padding, yFooter, boxW, 1, COLOR_DIM);

    // YOU Box
    std::string displayUserStr = ">> " + userMessage;
    if (showCursor && cursorTimer < 0.5f) displayUserStr += "_";
    font->draw(renderer, padding + 10, yUser + 10, "YOU:", COLOR_CYAN);
    drawWrappedText(renderer, font, displayUserStr, padding + 10, yUser + 35, boxW - 20, COLOR_WHITE);

    // AI Box (Với tính năng Auto-Scroll Terminal)
    font->draw(renderer, padding + 10, yAI + 10, "AMT ASSIST:", COLOR_PINK);
    std::string textToDraw = aiMessage.substr(0, charactersToShow);
    
    // Đo chiều cao tổng cộng của đoạn text
    int totalH = drawWrappedText(renderer, font, textToDraw, padding + 10, yAI + 35, boxW - 20, COLOR_WHITE, true);
    int startY = yAI + 35;
    
    // Nếu chữ dài hơn khung (tính từ startY đến hết hAI), ta đẩy Y lên trên
    if (totalH > hAI - 45) {
        startY -= (totalH - (hAI - 45));
    }
    
    // Dùng ClipRect để cắt những phần chữ bị trào ra ngoài AI Box
    SDL_Rect clipRect = { padding, yAI + 30, boxW, hAI - 35 };
    SDL_RenderSetClipRect(renderer, &clipRect);
    
    drawWrappedText(renderer, font, textToDraw, padding + 10, startY, boxW - 20, COLOR_WHITE);
    
    // Tắt ClipRect
    SDL_RenderSetClipRect(renderer, NULL);
}

void UiRenderer::render(int screenWidth, int screenHeight) {
    // Vẽ background
    fillRect(0, 0, screenWidth, screenHeight, COLOR_BG);

    int padding = 15;
    int boxW = screenWidth - padding * 2;
    
    // Khung viền ngoài cùng
    drawRect(padding, 10, boxW, screenHeight - 20, COLOR_DIM);
    drawRect(padding-1, 9, boxW+2, screenHeight - 18, COLOR_DIM);

    // Header
    font->draw(renderer, padding + 10, 15, "[Online]", {255, 200, 0, 255}); // Vàng Cyberpunk
    std::string header = "AMT ASSIST";
    int hw = font->getTextWidth(renderer, header);
    font->draw(renderer, (screenWidth - hw) / 2, 15, header, COLOR_CYAN);
    
    std::string rightHeader = "[Gemini 3.5]";
    int rhw = font->getTextWidth(renderer, rightHeader);
    font->draw(renderer, screenWidth - padding - 10 - rhw, 15, rightHeader, COLOR_PINK);

    // Avatar
    drawFace(screenWidth / 2, 130);

    // 2 Khung Chat & Divider
    drawTextBoxes(screenWidth, screenHeight);
    
    // Footer Tiếng Việt Có Dấu
    int yFooter = 220 + 70 + 120 + 10;
    RGBA COLOR_GRAY = {150, 150, 150, 255};
    font->draw(renderer, padding + 10, yFooter, "[BÀN PHÍM CƠ: Gõ chữ trực tiếp]", COLOR_GRAY);
    std::string f1r = "[ENTER: Gửi câu]";
    font->draw(renderer, screenWidth - padding - 10 - font->getTextWidth(renderer, f1r), yFooter, f1r, COLOR_GRAY);

    font->draw(renderer, padding + 10, yFooter + 25, "[D-PAD: Cuộn câu hỏi mẫu]  [A: Xác nhận]", COLOR_GRAY);
    std::string f2r = "[B/ESC: Thoát App]";
    font->draw(renderer, screenWidth - padding - 10 - font->getTextWidth(renderer, f2r), yFooter + 25, f2r, COLOR_GRAY);
}
