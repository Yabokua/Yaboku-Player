#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <portaudio.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

struct Track {
    std::string filepath;
    float durationSeconds = 0.0f;
};

struct Playlist {
    std::string name;
    std::vector<Track> tracks;
};

struct AudioInfo {
    float sampleRate = 44100.0f;
    int channels = 2;
    float duration = 0.0f;
    bool valid = false;
};

// Global variables for UI and state
extern std::vector<std::string> lyricsLines;
extern int currentLineIndex;
extern int selectedTrack;
extern std::string lastTrackPath;
extern std::mutex lyricsMutex;
extern std::atomic<bool> loadingLyrics;
extern std::vector<Track> playlist;
extern const std::string VOLUME_CONFIG_PATH;
extern float currentTrackPosition;
extern float currentTrackDuration;
extern int currentTrackIndex;
extern bool isSeeking;
extern const std::string K_PLAYLIST_FILENAME;
extern bool playlistChanged;
extern bool repeatEnabled;
extern std::filesystem::path resourcePath;                                                                                                                                                               
extern std::filesystem::path configPath;

// Global variables for audio
extern int current_volume;
extern bool shuffleEnabled;
extern std::vector<int> shuffleHistory;

void initAudioPlayer();
void shutdownAudio();
void addTrack(const std::string& filepath);
const std::vector<Track>& getPlaylist();
void clearPlaylist();
void playTrack(int index);
void pause();
void resume();
void stop();
bool isPlaying_f();
bool isPaused_f();
void playPrevious();
void playNext();
void toggleShuffle();
bool isShuffleEnabled();
void setVolume(float normalized);
float getNormalizedVolume();
void seekTo(float seconds);
AudioInfo getAudioInfo(const std::string& filepath);
std::vector<float> loadAudioWithFFmpeg(const std::string& filepath);
float getTrackDuration(const std::string& filepath);
double getTimeSec();
void updateTrackPosition();
void updatePlayback();
void verifyPlayback();
void loadLyricsAsync(const std::string& trackPath);
void loadVolumeFromFile();
void saveVolumeToFile();
void loadPlaylistsFromFile();
void savePlaylistsToFile();
const std::vector<Playlist>& getPlaylists();
std::vector<Playlist>& getPlaylistsMutable();
void addPlaylist(const std::string& name);
void addTrackToPlaylist(const std::string& playlistName, const Track& track);
void selectPlaylist(int index);
void addTrackToSelectedPlaylist(const Track& track);
void to_json(json& j, const Track& t);
void from_json(const json& j, Track& t);
void to_json(json& j, const Playlist& p);
void from_json(const json& j, Playlist& p);
void initializePaths();

