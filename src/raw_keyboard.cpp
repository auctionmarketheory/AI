#include "raw_keyboard.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>
#include <cstring>
#include <vector>

// Key mapping based on linux/input-event-codes.h
const std::map<int, char> KEY_MAP = {
    {11, '0'}, {2, '1'}, {3, '2'}, {4, '3'}, {5, '4'},
    {6, '5'}, {7, '6'}, {8, '7'}, {9, '8'}, {10, '9'},
    {30, 'a'}, {48, 'b'}, {46, 'c'}, {32, 'd'}, {18, 'e'},
    {33, 'f'}, {34, 'g'}, {35, 'h'}, {23, 'i'}, {36, 'j'},
    {37, 'k'}, {38, 'l'}, {50, 'm'}, {49, 'n'}, {24, 'o'},
    {25, 'p'}, {16, 'q'}, {19, 'r'}, {31, 's'}, {20, 't'},
    {22, 'u'}, {47, 'v'}, {17, 'w'}, {45, 'x'}, {21, 'y'}, {44, 'z'},
    {57, ' '}, {12, '-'}, {13, '='}, {26, '['}, {27, ']'},
    {39, ';'}, {40, '\''}, {43, '\\'}, {51, ','}, {52, '.'}, {53, '/'}
};

RawKeyboard::RawKeyboard() 
    : m_running(false), m_backspacePending(false), m_enterPending(false), m_escPending(false) {
}

RawKeyboard::~RawKeyboard() {
    stop();
}

int RawKeyboard::findKeyboardDevice() {
    DIR *dir = opendir("/dev/input");
    if (!dir) return -1;
    
    struct dirent *entry;
    int kbd_fd = -1;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            std::string path = std::string("/dev/input/") + entry->d_name;
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                unsigned long bitmask[EV_MAX/8 + 1];
                memset(bitmask, 0, sizeof(bitmask));
                ioctl(fd, EVIOCGBIT(0, sizeof(bitmask)), bitmask);
                
                // Kiểm tra xem device có hỗ trợ sự kiện phím (EV_KEY) không
                if (bitmask[0] & (1 << EV_KEY)) {
                    // Kiểm tra tiếp xem có hỗ trợ phím A, B, C không (loại trừ power button hoặc gamepad)
                    unsigned long keybit[KEY_MAX/8 + 1];
                    memset(keybit, 0, sizeof(keybit));
                    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
                    
                    if ((keybit[KEY_A/8] & (1 << (KEY_A%8))) && (keybit[KEY_SPACE/8] & (1 << (KEY_SPACE%8)))) {
                        // Đây khả năng cao là Bàn phím thực sự
                        kbd_fd = fd;
                        break;
                    }
                }
                close(fd);
            }
        }
    }
    closedir(dir);
    return kbd_fd;
}

void RawKeyboard::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&RawKeyboard::threadLoop, this);
}

void RawKeyboard::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void RawKeyboard::threadLoop() {
    int fd = -1;
    
    while (m_running) {
        if (fd < 0) {
            fd = findKeyboardDevice();
            if (fd < 0) {
                std::this_thread::sleep_for(std::chrono::seconds(1)); // Đợi nếu chưa cắm phím
                continue;
            }
            // Chuyển fd sang chế độ blocking để read không tốn CPU
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        }
        
        struct input_event ev;
        int rd = read(fd, &ev, sizeof(ev));
        
        if (rd < (int)sizeof(ev)) {
            // Lỗi đọc (có thể do rút cáp)
            close(fd);
            fd = -1;
            continue;
        }
        
        if (ev.type == EV_KEY && ev.value == 1) { // 1 = Key Press, (2 = Auto Repeat)
            std::lock_guard<std::mutex> lock(m_mutex);
            
            if (ev.code == KEY_BACKSPACE) {
                m_backspacePending = true;
            } else if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) {
                m_enterPending = true;
            } else if (ev.code == KEY_ESC) {
                m_escPending = true;
            } else if (KEY_MAP.count(ev.code)) {
                // Tạm thời chỉ hỗ trợ gõ chữ thường, chưa hỗ trợ Shift
                m_pendingText += KEY_MAP.at(ev.code);
            }
        }
    }
    
    if (fd >= 0) {
        close(fd);
    }
}

void RawKeyboard::updateInput(std::string& currentInput, bool& isTyping, bool& enterPressed, bool& escPressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_escPending) {
        escPressed = true;
        m_escPending = false;
    }
    if (m_enterPending) {
        enterPressed = true;
        m_enterPending = false;
    }
    if (m_backspacePending) {
        if (!currentInput.empty()) {
            currentInput.pop_back(); // Xóa 1 ký tự ASCII (chưa tính đến việc lùi UTF-8 nhiều byte nếu không có dấu)
        }
        m_backspacePending = false;
        isTyping = true;
    }
    
    if (!m_pendingText.empty()) {
        currentInput += m_pendingText;
        m_pendingText = "";
        isTyping = true;
    }
}
