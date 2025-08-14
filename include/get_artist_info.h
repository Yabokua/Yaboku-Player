#pragma once

#include <string>
#include <atomic>
#include <mutex>

void loadArtistInfoAsync(const std::string& artistName);
extern std::string artistBio;
extern std::atomic<bool> loadingArtistInfo;
extern std::mutex artistInfoMutex;

