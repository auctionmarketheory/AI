#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <string>

class ApiClient {
public:
    // Gửi prompt tới Gemini và trả về text kết quả.
    // Nếu lỗi, trả về chuỗi báo lỗi.
    static std::string askGemini(const std::string& prompt);

private:
    static std::string getApiKey();
};

#endif
