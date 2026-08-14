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

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Khong the khoi tao SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    int screenWidth = 640;
    int screenHeight = 480;

    SDL_Window* window = SDL_CreateWindow("R36S Tamagotchi", 
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

    CustomFont font;
    if (!font.load(renderer, "res/NotoSans-Regular.ttf", 24.0f)) {
        std::cerr << "Loi load font!\n";
    }

    UiRenderer ui(renderer, &font);
    ui.setExpression(FaceExpression::IDLE);
    ui.setChatMessage("Xin chao, toi la Xiaozhi R36S. Nhan UP/DOWN de chon cau hoi, nhan ENTER de gui!");

    bool running = true;
    SDL_Event e;
    Uint32 lastTime = SDL_GetTicks();

    // Danh sách câu hỏi giả lập (vì không có mic)
    std::vector<std::string> prompts = {
        "Hay ke mot cau chuyen ngan ve hacker.",
        "Toi dang cam thay buon, ban co the an ui toi khong?",
        "Ban nghi sao ve he dieu hanh Linux?",
        "Ban la ai va tao ra voi muc dich gi?"
    };
    int currentPromptIndex = 0;

    bool waitingForNetwork = false;
    std::thread networkThread;

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                
                if (!isFetching) {
                    if (e.key.keysym.sym == SDLK_UP) {
                        currentPromptIndex--;
                        if (currentPromptIndex < 0) currentPromptIndex = prompts.size() - 1;
                        ui.setChatMessage(">> " + prompts[currentPromptIndex]);
                        ui.setExpression(FaceExpression::IDLE);
                    } else if (e.key.keysym.sym == SDLK_DOWN) {
                        currentPromptIndex++;
                        if (currentPromptIndex >= prompts.size()) currentPromptIndex = 0;
                        ui.setChatMessage(">> " + prompts[currentPromptIndex]);
                        ui.setExpression(FaceExpression::IDLE);
                    } else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
                        ui.setExpression(FaceExpression::THINKING);
                        ui.setChatMessage("Dang ket noi ve tinh...");
                        waitingForNetwork = true;
                        
                        if (networkThread.joinable()) {
                            networkThread.join();
                        }
                        // Thêm đuôi để ép Gemini trả lời tiếng Việt không dấu (vì font hiện tại decode UTF8 nhưng bộ font NotoSans có thể bị thiếu glyph tiếng Việt nếu không dùng đúng bản Tiếng Việt, tốt nhất là gõ tiếng Việt không dấu hoặc tiếng Việt chuẩn tùy font).
                        networkThread = std::thread(fetchGeminiTask, prompts[currentPromptIndex] + " (Tra loi ngan gon duoi 30 chu).");
                    }
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
