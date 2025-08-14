#include "lyrics.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <string>
#include <array>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::vector<std::string> lyricsLines;
static int currentLineIndex = 0;
static std::mutex lyricsMutex;
static std::atomic<bool> loadingLyrics{false};

//Get free Api on Genius.com
std::string geniusApiToken = "Bearer 7SYNVMQ2E_xrI7Mj0V9IkZ_bthDrVw8g2y2q6O-OhzWNAtXafb_uMi7XYap4m_lf";

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string searchSongOnGenius(const std::string& artist, const std::string& title) {
    CURL* curl = curl_easy_init();
    std::string result;

    if (curl) {
        char* escaped_query = curl_easy_escape(curl, (artist + " " + title).c_str(), 0);
        std::string query = "https://api.genius.com/search?q=" + std::string(escaped_query);

        curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + geniusApiToken).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }

    return result;
}

// Get track's lyrics URL
std::string extractLyricsUrl(const std::string& jsonResponse) {
        json j = json::parse(jsonResponse);

        if (j.contains("response") && j["response"].contains("hits") &&
            j["response"]["hits"].is_array() && !j["response"]["hits"].empty()) {
            return j["response"]["hits"][0]["result"]["url"].get<std::string>();
        }
    return "";
}

std::string fetchLyricsFromUrl(const std::string& url) {
    fs::path scriptPath = fs::path(PROJECT_ROOT_DIR) / "scripts" / "get_lyrics.py";
    
    if (!fs::exists(scriptPath)) {
        std::cerr << "Error: Lyrics script not found at " << scriptPath << std::endl;
        return "Lyrics script not found.";
    }

    std::string command = "python3 \"" + scriptPath.string() + "\" \"" + url + "\"";

    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: Failed to execute lyrics script: " << strerror(errno) << std::endl;
        return "Failed to fetch lyrics.";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    if (status != 0) {
        std::cerr << "Error: Lyrics script exited with status " << status << std::endl;
        return "Failed to fetch lyrics.";
    }

    return result.empty() ? "No lyrics found." : result;
}
