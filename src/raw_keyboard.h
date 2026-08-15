#ifndef RAW_KEYBOARD_H
#define RAW_KEYBOARD_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>

class RawKeyboard {
public:
    RawKeyboard();
    ~RawKeyboard();

    void start();
    void stop();

    // Lấy ký tự đã gõ và các phím điều khiển
    void updateInput(std::string& currentInput, bool& isTyping, bool& enterPressed, bool& escPressed);

private:
    void threadLoop();
    std::vector<int> findKeyboardDevices();

    std::atomic<bool> m_running;
    std::thread m_thread;
    std::mutex m_mutex;
    
    std::vector<int> m_fds;

    std::string m_pendingText;
    bool m_backspacePending;
    bool m_enterPending;
    bool m_escPending;

    std::map<int, char> m_keyMap;
};

#endif
