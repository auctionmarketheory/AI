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

std::vector<int> RawKeyboard::findKeyboardDevices() {
    std::vector<int> fds;
    DIR *dir = opendir("/dev/input");
    if (!dir) return fds;
    
    struct dirent *entry;
    
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
                    unsigned long keybit[KEY_MAX/(sizeof(unsigned long)*8) + 1];
                    memset(keybit, 0, sizeof(keybit));
                    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
                    
                    int bitIndexA = KEY_A / (sizeof(unsigned long) * 8);
                    int bitOffsetA = KEY_A % (sizeof(unsigned long) * 8);
                    int bitIndexSpace = KEY_SPACE / (sizeof(unsigned long) * 8);
                    int bitOffsetSpace = KEY_SPACE % (sizeof(unsigned long) * 8);
                    
                    if ((keybit[bitIndexA] & (1UL << bitOffsetA)) && (keybit[bitIndexSpace] & (1UL << bitOffsetSpace))) {
                        // Lưu toàn bộ các đường truyền (Endpoint) thoả mãn
                        fds.push_back(fd);
                    } else {
                        close(fd);
                    }
                } else {
                    close(fd);
                }
            }
        }
    }
    closedir(dir);
    return fds;
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
    while (m_running) {
        if (m_fds.empty()) {
            m_fds = findKeyboardDevices();
            if (m_fds.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(1)); // Đợi nếu chưa cắm phím
                continue;
            }
        }
        
        bool anyEventRead = false;
        
        for (auto it = m_fds.begin(); it != m_fds.end(); ) {
            int fd = *it;
            struct input_event ev;
            int rd = read(fd, &ev, sizeof(ev));
            
            if (rd > 0) {
                anyEventRead = true;
                if (ev.type == EV_KEY && ev.value == 1) { // 1 = Key Press
                    std::lock_guard<std::mutex> lock(m_mutex);
                    
                    if (ev.code == KEY_BACKSPACE) {
                        m_backspacePending = true;
                    } else if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) {
                        m_enterPending = true;
                    } else if (ev.code == KEY_ESC) {
                        m_escPending = true;
                    } else if (KEY_MAP.count(ev.code)) {
                        m_pendingText += KEY_MAP.at(ev.code);
                    }
                }
                ++it;
            } else if (rd < 0 && errno == EAGAIN) {
                // FD này đang rảnh rỗi (Không có lỗi, không có event)
                ++it;
            } else {
                // Thiết bị bị rút hoặc lỗi nghiêm trọng
                close(fd);
                it = m_fds.erase(it);
            }
        }
        
        // Nếu đã quét qua toàn bộ FD mà không có bất kỳ event nào, ta ngủ 10ms để tiết kiệm CPU
        if (!anyEventRead) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    for (int fd : m_fds) {
        close(fd);
    }
    m_fds.clear();
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
