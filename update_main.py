import re

with open("src/main.cpp", "r", encoding="utf-8") as f:
    code = f.read()

# Add joystick definitions and include cmath
code = code.replace("int main(int argc, char* argv[]) {", """#include <cmath>

// ─── Joystick Buttons (R36S layout) ──────────────────────
#define BTN_A  0
#define BTN_B  1
#define BTN_X  2
#define BTN_Y  3
#define BTN_DPAD_UP    8
#define BTN_DPAD_DOWN  9
#define BTN_DPAD_LEFT  10
#define BTN_DPAD_RIGHT 11

int main(int argc, char* argv[]) {""")

# Update initialization
code = code.replace("SDL_Init(SDL_INIT_VIDEO)", "SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)")

# Replace the middle part with lambdas and accented text
target_middle = """    CustomFont font;
    if (!font.load(renderer, "res/NotoSans-Regular.ttf", 24.0f)) {
        std::cerr << "Loi load font!\\n";
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
    std::thread networkThread;"""

replacement_middle = """    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    CustomFont font;
    if (!font.load(renderer, "res/NotoSans-Regular.ttf", 24.0f)) {
        std::cerr << "Lỗi load font!\\n";
    }

    UiRenderer ui(renderer, &font);
    ui.setExpression(FaceExpression::IDLE);
    ui.setChatMessage("Xin chào, tôi là Xiaozhi R36S. Dùng D-PAD để chọn câu hỏi, nhấn nút A để gửi!");

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

    bool waitingForNetwork = false;
    std::thread networkThread;

    auto handleActionUp = [&]() {
        if (isFetching) return;
        currentPromptIndex--;
        if (currentPromptIndex < 0) currentPromptIndex = prompts.size() - 1;
        ui.setChatMessage(">> " + prompts[currentPromptIndex]);
        ui.setExpression(FaceExpression::IDLE);
    };

    auto handleActionDown = [&]() {
        if (isFetching) return;
        currentPromptIndex++;
        if (currentPromptIndex >= prompts.size()) currentPromptIndex = 0;
        ui.setChatMessage(">> " + prompts[currentPromptIndex]);
        ui.setExpression(FaceExpression::IDLE);
    };

    auto handleActionConfirm = [&]() {
        if (isFetching) return;
        ui.setExpression(FaceExpression::THINKING);
        ui.setChatMessage("Đang kết nối vệ tinh...");
        waitingForNetwork = true;
        
        if (networkThread.joinable()) {
            networkThread.join();
        }
        networkThread = std::thread(fetchGeminiTask, prompts[currentPromptIndex] + " (Trả lời ngắn gọn dưới 30 chữ, dùng Tiếng Việt có dấu).");
    };

    auto handleActionQuit = [&]() {
        running = false;
    };"""

code = code.replace(target_middle, replacement_middle)

target_loop = """        while (SDL_PollEvent(&e)) {
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
        }"""

replacement_loop = """        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                handleActionQuit();
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) handleActionQuit();
                else if (e.key.keysym.sym == SDLK_UP) handleActionUp();
                else if (e.key.keysym.sym == SDLK_DOWN) handleActionDown();
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) handleActionConfirm();
            } else if (e.type == SDL_JOYBUTTONDOWN) {
                if (e.jbutton.button == BTN_B) handleActionQuit();
                else if (e.jbutton.button == BTN_A) handleActionConfirm();
                else if (e.jbutton.button == BTN_DPAD_UP) handleActionUp();
                else if (e.jbutton.button == BTN_DPAD_DOWN) handleActionDown();
            } else if (e.type == SDL_JOYHATMOTION) {
                if (e.jhat.value == SDL_HAT_UP) handleActionUp();
                else if (e.jhat.value == SDL_HAT_DOWN) handleActionDown();
            } else if (e.type == SDL_JOYAXISMOTION) {
                static bool axisActive[4] = {false, false, false, false};
                int axis = e.jaxis.axis;
                Sint16 val = e.jaxis.value;
                const Sint16 DEAD = 8000;
                if (axis == 1) { // Vertical
                    if (val < -DEAD && !axisActive[2]) { handleActionUp(); axisActive[2] = true; }
                    else if (val > DEAD && !axisActive[3]) { handleActionDown(); axisActive[3] = true; }
                    else if (std::abs(val) <= DEAD) { axisActive[2] = false; axisActive[3] = false; }
                }
            }
        }"""

code = code.replace(target_loop, replacement_loop)

with open("src/main.cpp", "w", encoding="utf-8") as f:
    f.write(code)

print("DONE python replace")
