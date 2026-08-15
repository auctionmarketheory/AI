import re

with open("src/main.cpp", "r", encoding="utf-8") as f:
    code = f.read()

# Add debounce timer
target_vars = """    int currentPromptIndex = 0;

    bool waitingForNetwork = false;
    std::thread networkThread;"""

replacement_vars = """    int currentPromptIndex = 0;

    bool waitingForNetwork = false;
    std::thread networkThread;
    
    Uint32 lastInputTime = 0;
    const Uint32 DEBOUNCE_MS = 250;"""

code = code.replace(target_vars, replacement_vars)

# Update lambdas to include debounce
target_up = """    auto handleActionUp = [&]() {
        if (isFetching) return;
        currentPromptIndex--;"""

replacement_up = """    auto handleActionUp = [&]() {
        if (isFetching || SDL_GetTicks() - lastInputTime < DEBOUNCE_MS) return;
        lastInputTime = SDL_GetTicks();
        currentPromptIndex--;"""

code = code.replace(target_up, replacement_up)

target_down = """    auto handleActionDown = [&]() {
        if (isFetching) return;
        currentPromptIndex++;"""

replacement_down = """    auto handleActionDown = [&]() {
        if (isFetching || SDL_GetTicks() - lastInputTime < DEBOUNCE_MS) return;
        lastInputTime = SDL_GetTicks();
        currentPromptIndex++;"""

code = code.replace(target_down, replacement_down)

target_confirm = """    auto handleActionConfirm = [&]() {
        if (isFetching) return;
        ui.setExpression(FaceExpression::THINKING);"""

replacement_confirm = """    auto handleActionConfirm = [&]() {
        if (isFetching || SDL_GetTicks() - lastInputTime < DEBOUNCE_MS) return;
        lastInputTime = SDL_GetTicks();
        ui.setExpression(FaceExpression::THINKING);"""

code = code.replace(target_confirm, replacement_confirm)

# Increase deadzone
code = code.replace("const Sint16 DEAD = 8000;", "const Sint16 DEAD = 16000;")

with open("src/main.cpp", "w", encoding="utf-8") as f:
    f.write(code)

print("DONE python fix")
