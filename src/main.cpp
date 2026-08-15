#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include "CustomFont.h"
#include "ui_renderer.h"
#include "api_client.h"

// Biến toàn cục cho luồng mạng
std::atomic<bool> isFetching(false);
std::string apiResponse = "";

// Hàm chạy trên luồng phụ
void fetchGeminiTask(std::string prompt) {
    isFetching = true;
    apiResponse = ApiClient::askGemini(prompt);
    isFetching = false;
}

#include <cmath>

// ─── Joystick Buttons (R36S layout) ──────────────────────
#define BTN_A  0
#define BTN_B  1
#define BTN_X  2
#define BTN_Y  3
#define BTN_SELECT 8
#define BTN_START  9
#define BTN_DPAD_UP    13
#define BTN_DPAD_DOWN  14
#define BTN_DPAD_LEFT  15
#define BTN_DPAD_RIGHT 16

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "Khong the khoi tao SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    int screenWidth = 640;
    int screenHeight = 480;

    SDL_Window* window = SDL_CreateWindow("Trợ lý AMT", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        screenWidth, screenHeight, SDL_WINDOW_SHOWN);
    
    if (!window) {
        std::cerr << "Loi tao cua so: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Loi tao renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    CustomFont font;
    if (!font.load(renderer, "res/NotoSans-Regular.ttf", 24.0f)) {
        std::cerr << "Lỗi load font!\n";
    }

    UiRenderer ui(renderer, &font);
    ui.setExpression(FaceExpression::IDLE);
    ui.setChatMessage("Xin chào, tôi là trợ lý AMT. Dùng D-PAD để chọn câu hỏi, nhấn nút A để gửi!");

    bool running = true;
    SDL_Event e;
    Uint32 lastTime = SDL_GetTicks();

    // Danh sách câu hỏi giả lập (vì không có mic)
    std::vector<std::string> prompts = {
        "Hãy kể một câu chuyện ngắn về hacker.",
        "Tôi đang cảm thấy buồn, bạn có thể an ủi tôi không?",
        "Bạn nghĩ sao về hệ điều hành Linux?",
        "Bạn là ai và được tạo ra với mục đích gì?"
    };
    int currentPromptIndex = 0;

    std::string userInput = "";
    bool isTypingMode = false;
    SDL_StartTextInput();

    bool waitingForNetwork = false;
    std::thread networkThread;
    
    Uint32 lastInputTime = 0;
    const Uint32 DEBOUNCE_MS = 250;

    auto handleActionUp = [&]() {
        if (isFetching || SDL_GetTicks() - lastInputTime < DEBOUNCE_MS) return;
        lastInputTime = SDL_GetTicks();
        isTypingMode = false;
        currentPromptIndex--;
        if (currentPromptIndex < 0) currentPromptIndex = prompts.size() - 1;
        ui.setChatMessage(">> " + prompts[currentPromptIndex]);
        ui.setExpression(FaceExpression::IDLE);
    };

    auto handleActionDown = [&]() {
        if (isFetching || SDL_GetTicks() - lastInputTime < DEBOUNCE_MS) return;
        lastInputTime = SDL_GetTicks();
        isTypingMode = false;
        currentPromptIndex++;
        if (currentPromptIndex >= prompts.size()) currentPromptIndex = 0;
        ui.setChatMessage(">> " + prompts[currentPromptIndex]);
        ui.setExpression(FaceExpression::IDLE);
    };

    auto handleActionConfirm = [&]() {
        if (isFetching || SDL_GetTicks() - lastInputTime < DEBOUNCE_MS) return;
        if (isTypingMode && userInput.empty()) return;
        lastInputTime = SDL_GetTicks();
        ui.setExpression(FaceExpression::THINKING);
        ui.setChatMessage("Đang kết nối vệ tinh...");
        waitingForNetwork = true;
        
        if (networkThread.joinable()) {
            networkThread.join();
        }
        std::string textToSubmit = isTypingMode ? userInput : prompts[currentPromptIndex];
        if (isTypingMode) {
            userInput = "";
            isTypingMode = false;
        }
        networkThread = std::thread(fetchGeminiTask, textToSubmit + " (Trả lời ngắn gọn dưới 30 chữ, dùng Tiếng Việt có dấu).");
    };

    auto handleActionQuit = [&]() {
        running = false;
    };

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                handleActionQuit();
            } else if (e.type == SDL_TEXTINPUT) {
                if (userInput.length() < 200) {
                    userInput += e.text.text;
                    isTypingMode = true;
                    ui.setChatMessage(">> " + userInput + "_");
                }
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    if (isTypingMode && !userInput.empty()) {
                        // Xóa 1 byte, loop tiếp nếu là UTF-8 extension byte (10xxxxxx)
                        do {
                            userInput.pop_back();
                        } while (!userInput.empty() && (userInput.back() & 0xC0) == 0x80);
                        ui.setChatMessage(">> " + userInput + "_");
                    }
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_b) handleActionQuit();
                else if (e.key.keysym.sym == SDLK_UP) handleActionUp();
                else if (e.key.keysym.sym == SDLK_DOWN) handleActionDown();
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_a) handleActionConfirm();
            } else if (e.type == SDL_JOYBUTTONDOWN) {
                if (e.jbutton.button == BTN_B) handleActionQuit();
                else if (e.jbutton.button == BTN_A) handleActionConfirm();
                else if (e.jbutton.button == BTN_DPAD_UP || e.jbutton.button == BTN_SELECT) handleActionUp();
                else if (e.jbutton.button == BTN_DPAD_DOWN || e.jbutton.button == BTN_START) handleActionDown();
            } else if (e.type == SDL_JOYHATMOTION) {
                if (e.jhat.value & SDL_HAT_UP) handleActionUp();
                else if (e.jhat.value & SDL_HAT_DOWN) handleActionDown();
            } else if (e.type == SDL_JOYAXISMOTION) {
                static bool axisActive[4] = {false, false, false, false};
                int axis = e.jaxis.axis;
                Sint16 val = e.jaxis.value;
                const Sint16 DEAD = 16000;
                // Axis 1 là Left Stick Vertical, Axis 7 có thể là D-Pad Vertical trên một số Firmware ArkOS
                if (axis == 1 || axis == 7 || axis == 5) { // Vertical
                    if (val < -DEAD && !axisActive[2]) { handleActionUp(); axisActive[2] = true; }
                    else if (val > DEAD && !axisActive[3]) { handleActionDown(); axisActive[3] = true; }
                    else if (abs(val) <= DEAD) { axisActive[2] = false; axisActive[3] = false; }
                }
            }

        }

        if (waitingForNetwork && !isFetching) {
            waitingForNetwork = false;
            if (apiResponse.find("Loi") != std::string::npos) {
                ui.setExpression(FaceExpression::ERROR);
            } else {
                ui.setExpression(FaceExpression::TALKING);
            }
            ui.setChatMessage(apiResponse);
        }

        ui.update(deltaTime);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        ui.render(screenWidth, screenHeight);

        SDL_RenderPresent(renderer);
    }

    if (networkThread.joinable()) {
        networkThread.join();
    }

    font.freeCache();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
