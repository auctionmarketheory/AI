#include "api_client.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string ApiClient::getApiKey() {
    std::ifstream file(".env");
    std::string key;
    if (file.is_open()) {
        std::getline(file, key);
        // Trim whitespace and newlines
        key.erase(key.find_last_not_of(" \n\r\t") + 1);
    }
    return key;
}

std::string ApiClient::askGemini(const std::string& prompt) {
    std::string api_key = getApiKey();
    if (api_key.empty()) {
        return "Loi: Khong tim thay API Key trong file .env!";
    }

    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.5-flash:generateContent?key=" + api_key;
        
        json requestData = {
            {"contents", {
                {
                    {"parts", {
                        {{"text", prompt}}
                    }}
                }
            }}
        };
        std::string jsonStr = requestData.dump();

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // CRITICAL BYPASS SSL FOR R36S (MISSING CERTS IN ARKOS)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        // Timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if(res != CURLE_OK) {
            return "Loi Mang: " + std::string(curl_easy_strerror(res));
        }

        try {
            json responseData = json::parse(readBuffer);
            if (responseData.contains("candidates") && responseData["candidates"].size() > 0) {
                auto& candidate = responseData["candidates"][0];
                if (candidate.contains("content") && candidate["content"].contains("parts") && candidate["content"]["parts"].size() > 0) {
                    std::string text = candidate["content"]["parts"][0]["text"].get<std::string>();
                    // Xóa ký tự newline thừa ở cuối
                    text.erase(text.find_last_not_of(" \n\r\t") + 1);
                    return text;
                }
            } else if (responseData.contains("error")) {
                if (responseData["error"].contains("message")) {
                    return "Loi API: " + responseData["error"]["message"].get<std::string>();
                }
            }
            return "Loi: Parse JSON tra ve khong dung cau truc.";
        } catch (json::parse_error& e) {
            return "Loi Parse JSON: " + std::string(e.what());
        }
    }
    return "Loi: Khong the khoi tao libcurl.";
}
