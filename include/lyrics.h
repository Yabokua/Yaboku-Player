#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <regex>

using json = nlohmann::json;

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
std::string searchSongOnGenius(const std::string& artist, const std::string& title);
std::string extractLyricsUrl(const std::string& jsonResponse);
std::string fetchLyricsFromUrl(const std::string& url);

