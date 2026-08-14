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

void UiRenderer::setChatMessage(const std::string& msg) {
    currentMessage = msg;
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

    // Logic gõ phím (Typewriter)
    if (charactersToShow < currentMessage.length()) {
        typewriterTimer += deltaTime;
        if (typewriterTimer > 0.02f) { // Tốc độ gõ 0.02s / ký tự
            charactersToShow++;
            typewriterTimer = 0.0f;
            
            // Fix utf8 split (không cắt ngang ký tự utf-8)
            // Kỹ thuật đơn giản: Cứ duyệt xem nếu gặp bit 10xxxxxx thì hiển thị thêm 1 byte nữa
            while (charactersToShow < currentMessage.length() && 
                  (currentMessage[charactersToShow] & 0xC0) == 0x80) {
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

void UiRenderer::drawTextBox(int screenWidth, int screenHeight) {
    int boxX = 20;
    int boxY = screenHeight - 140;
    int boxW = screenWidth - 40;
    int boxH = 120;

    // Khung viền Neon
    fillRect(boxX, boxY, boxW, boxH, COLOR_DIM);
    drawRect(boxX, boxY, boxW, boxH, COLOR_CYAN);
    drawRect(boxX-1, boxY-1, boxW+2, boxH+2, COLOR_PINK);

    // Lấy chuỗi theo số ký tự typewriter
    std::string textToDraw = currentMessage.substr(0, charactersToShow);
    
    // Thuật toán bọc dòng (Word Wrap) thủ công để không tràn khỏi màn hình
    int textX = boxX + 15;
    int textY = boxY + 15;
    int maxWidth = boxW - 30;
    int lineHeight = font->sz + 10;

    std::string currentLine = "";
    size_t i = 0;
    while (i < textToDraw.length()) {
        // Tìm từ tiếp theo
        size_t nextSpace = textToDraw.find(' ', i);
        if (nextSpace == std::string::npos) nextSpace = textToDraw.length();
        
        std::string word = textToDraw.substr(i, nextSpace - i + 1); // +1 để bao gồm cả khoảng trắng
        
        // Kiểm tra chiều dài dòng hiện tại + từ mới
        int lineWidth = font->getTextWidth(renderer, currentLine + word);
        if (lineWidth > maxWidth && currentLine.length() > 0) {
            // Dòng đã đầy, vẽ dòng hiện tại và xuống dòng
            font->draw(renderer, textX, textY, currentLine, COLOR_WHITE);
            textY += lineHeight;
            currentLine = word;
        } else {
            currentLine += word;
        }
        i = nextSpace + 1;
    }
    // Vẽ dòng cuối
    if (currentLine.length() > 0) {
        font->draw(renderer, textX, textY, currentLine, COLOR_WHITE);
    }
}

void UiRenderer::render(int screenWidth, int screenHeight) {
    // Vẽ background
    fillRect(0, 0, screenWidth, screenHeight, COLOR_BG);

    // Vẽ Face ở giữa phía trên
    drawFace(screenWidth / 2, screenHeight / 2 - 60);

    // Vẽ Khung Chat
    drawTextBox(screenWidth, screenHeight);
}
