#include "get_artist_info.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <regex>
#include <string>

std::string artistBio;
std::atomic<bool> loadingArtistInfo = false;
std::mutex artistInfoMutex;

using json = nlohmann::json;

// Callback for writting data received from URL
size_t writeCallbackArtistInfo(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Clear json response
std::string prepareArtistSummary(const std::string& rawHtmlSummary) {
    size_t linkPos = rawHtmlSummary.find("<a href=");
    std::string noLink = (linkPos != std::string::npos) ? rawHtmlSummary.substr(0, linkPos) : rawHtmlSummary;

    // dalete gtml tags
    std::regex htmlTagRegex("<[^>]*>");
    std::string cleanText = std::regex_replace(noLink, htmlTagRegex, "");

    size_t first = cleanText.find_first_not_of(" \n\r\t");
    size_t last = cleanText.find_last_not_of(" \n\r\t");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return cleanText.substr(first, last - first + 1);
}

void loadArtistInfoAsync(const std::string& artistName) {
    loadingArtistInfo = true;

    std::thread([artistName]() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::lock_guard<std::mutex> lock(artistInfoMutex);
            artistBio = "Failed to initialize network.";
            loadingArtistInfo = false;
            return;
        }

        char* escapedName = curl_easy_escape(curl, artistName.c_str(), 0);
        if (!escapedName) {
            std::lock_guard<std::mutex> lock(artistInfoMutex);
            artistBio = "Failed to encode artist name.";
            curl_easy_cleanup(curl);
            loadingArtistInfo = false;
            return;
        }

        std::string url = std::string("http://ws.audioscrobbler.com/2.0/?method=artist.getinfo&artist=")
                        + escapedName
                        + "&api_key=00b467edf6ced86f44e9c9d784290cc4&format=json";

        curl_free(escapedName);

        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallbackArtistInfo);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::lock_guard<std::mutex> lock(artistInfoMutex);
            artistBio = "Failed to fetch artist info.";
            loadingArtistInfo = false;
            return;
        }

        try {
            json j = json::parse(response);
            std::string rawSummary = j["artist"]["bio"]["summary"];

            std::string cleanSummary = prepareArtistSummary(rawSummary);

            std::lock_guard<std::mutex> lock(artistInfoMutex);
            artistBio = cleanSummary;
        } catch (...) {
            std::lock_guard<std::mutex> lock(artistInfoMutex);
            artistBio = "Failed to parse artist info.";
        }

        loadingArtistInfo = false;
    }).detach();
}

