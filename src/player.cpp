#include "eq.h"
#include "player.h"
#include "ui.h"
#include "lyrics.h"
#include "texture_loader.h"

#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <memory>
#include <sstream>
#include <random>
#include <portaudio.h>
#include <cstring>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

int currentTrackIndex = -1;
int current_volume = 64;//from 0 to 128
int selectedPlaylistIndex = -1;
int selectedTrackIndex = -1;
int currentLineIndex = 0;
int selectedTrack = -1;
float currentTrackPosition = 0.0f;
float currentTrackDuration = 0.0f;
bool playlistChanged = false;
bool isSeeking = false;
bool repeatEnabled = false;
bool shuffleEnabled = false;
std::vector<Playlist> playlists;
std::vector<std::string> lyricsLines;
std::string lastTrackPath;
std::mutex lyricsMutex;
std::atomic<bool> loadingLyrics{false};
std::vector<int> shuffleHistory;
std::vector<Track> playlist;
std::filesystem::path resourcePath = std::filesystem::path(PROJECT_ROOT_DIR) / "resources";
std::filesystem::path configPath = std::filesystem::path(PROJECT_ROOT_DIR) / "config";

const std::string K_PLAYLIST_FILENAME = (configPath / "playlists.json").string();
const std::string VOLUME_CONFIG_PATH = (configPath / "volume.cfg").string();

// PortAudio variables
static PaStream* audioStream = nullptr;
static std::vector<float> audioBuffer;
static std::atomic<size_t> audioBufferPos{0};
static std::atomic<bool> isPlaying{false};
static std::atomic<bool> isPaused{false};
static std::mutex audioMutex;

// constants for audio
static const float TARGET_SAMPLE_RATE = 44100.0f;
static const int TARGET_CHANNELS = 2;
static const int FRAMES_PER_BUFFER = 512;

void initializePaths() {
    std::cout << "PROJECT_ROOT_DIR: " << PROJECT_ROOT_DIR << std::endl;
    
    fs::path rootPath(PROJECT_ROOT_DIR);
    fs::path configDir = rootPath / "config";
    fs::path tempDir = rootPath / "temp";
    fs::path resourcesDir = rootPath / "resources";
    
    try {
        if (!fs::exists(configDir)) {
            fs::create_directories(configDir);
            std::cout << "Created config directory: " << configDir << std::endl;
        }
        
        if (!fs::exists(tempDir)) {
            fs::create_directories(tempDir);
            std::cout << "Created temp directory: " << tempDir << std::endl;
        }
        
        if (!fs::exists(resourcesDir)) {
            fs::create_directories(resourcesDir);
            std::cout << "Created resources directory: " << resourcesDir << std::endl;
        }
        
        std::cout << "All directories initialized successfully!" << std::endl;
        
    } catch (const fs::filesystem_error& ex) {
        std::cerr << "Filesystem error: " << ex.what() << std::endl;
        std::cerr << "Path1: " << ex.path1() << std::endl;
        std::cerr << "Path2: " << ex.path2() << std::endl;
        throw;
    }
}

// Утилиты
float getNormalizedVolume() {
    return current_volume / 128.0f;
}

void setVolume(float normalized) {
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    current_volume = static_cast<int>(normalized * 128);
    saveVolumeToFile();
}

double getTimeSec() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// Get info about audio file via ffprobe
AudioInfo getAudioInfo(const std::string& filepath) {
    AudioInfo info;
    
    // Get sampling frequency
    std::string sampleRateCmd = "ffprobe -v quiet -select_streams a:0 -show_entries stream=sample_rate -of default=noprint_wrappers=1:nokey=1 \"" + filepath + "\" 2>/dev/null";
    std::unique_ptr<FILE, int(*)(FILE*)> pipe1(popen(sampleRateCmd.c_str(), "r"), pclose);
    if (pipe1) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe1.get()) != nullptr) {
            try {
                float sr = std::stof(std::string(buffer));
                if (sr > 0) {
                    info.sampleRate = sr;
                    info.valid = true;
                }
            } catch (...) {}
        }
    }
    
    // Get number of channels
    std::string channelsCmd = "ffprobe -v quiet -select_streams a:0 -show_entries stream=channels -of default=noprint_wrappers=1:nokey=1 \"" + filepath + "\" 2>/dev/null";
    std::unique_ptr<FILE, int(*)(FILE*)> pipe2(popen(channelsCmd.c_str(), "r"), pclose);
    if (pipe2) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe2.get()) != nullptr) {
            try {
                int ch = std::stoi(std::string(buffer));
                if (ch > 0) info.channels = ch;
            } catch (...) {}
        }
    }
    
    // Get duration
    std::string durationCmd = "ffprobe -v quiet -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + filepath + "\" 2>/dev/null";
    std::unique_ptr<FILE, int(*)(FILE*)> pipe3(popen(durationCmd.c_str(), "r"), pclose);
    if (pipe3) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe3.get()) != nullptr) {
            try {
                float dur = std::stof(std::string(buffer));
                if (dur > 0) info.duration = dur;
            } catch (...) {}
        }
    }
    
    return info;
}

// Loading audion via ffpmeg
std::vector<float> loadAudioWithFFmpeg(const std::string& filepath) {
    // create temp file
    std::string tempFile = "/tmp/audio_temp_" + std::to_string(rand()) + ".raw";
    
    // Extract RAW audio data in float32 format, stereo, 44100 Hz
    std::string command = "ffmpeg -i \"" + filepath + "\" -f f32le -acodec pcm_f32le -ac 2 -ar 44100 \"" + tempFile + "\" -y 2>/dev/null";
    
    std::cout << "Executing: " << command << std::endl;
    
    int result = system(command.c_str());
    if (result != 0) {
        std::cerr << "FFmpeg failed to convert audio (exit code: " << result << ")" << std::endl;
        return {};
    }
    
    if (!fs::exists(tempFile)) {
        std::cerr << "Temporary audio file was not created" << std::endl;
        return {};
    }
    
    // Read RAW data
    std::ifstream file(tempFile, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open temporary audio file: " << tempFile << std::endl;
        fs::remove(tempFile);
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize == 0) {
        std::cerr << "Temporary audio file is empty" << std::endl;
        file.close();
        fs::remove(tempFile);
        return {};
    }
    
    std::vector<float> audioData(fileSize / sizeof(float));
    file.read(reinterpret_cast<char*>(audioData.data()), fileSize);
    file.close();
    
    fs::remove(tempFile);
    
    std::cout << "Loaded " << audioData.size() << " audio samples (" 
              << audioData.size() / 2 << " frames)" << std::endl;
    
    return audioData;
}

void loadLyricsAsync(const std::string& trackPath) {
    if (loadingLyrics) return;
    if (trackPath.empty()) return;
    loadingLyrics = true;

    std::thread([trackPath]() {
        std::string title = getFileNameWithoutExtension(trackPath);
        std::string artist = "";
        std::string response = searchSongOnGenius(artist, title);
        std::string url = extractLyricsUrl(response);
        std::string lyricsText = fetchLyricsFromUrl(url);

        std::vector<std::string> newLyrics;
        std::istringstream ss(lyricsText);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) newLyrics.push_back(line);
        }

        {
            std::lock_guard<std::mutex> lock(lyricsMutex);
            lyricsLines = std::move(newLyrics);
            currentLineIndex = 0;
            lastTrackPath = trackPath;
        }
        loadingLyrics = false;
    }).detach();
}

// PortAudio callback
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void* userData) {
    
    float* out = (float*)outputBuffer;
    std::lock_guard<std::mutex> lock(audioMutex);
    
    // set 0 default
    for (unsigned long i = 0; i < framesPerBuffer * TARGET_CHANNELS; ++i) {
        out[i] = 0.0f;
    }
    
    if (isPlaying && !isPaused && !audioBuffer.empty()) {
        size_t currentPos = audioBufferPos.load();
        float volume = getNormalizedVolume();
        
        for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
            if (currentPos + 1 < audioBuffer.size()) {
                out[frame * TARGET_CHANNELS] = audioBuffer[currentPos] * volume;         // Left
                out[frame * TARGET_CHANNELS + 1] = audioBuffer[currentPos + 1] * volume; // Right
                currentPos += TARGET_CHANNELS;
            } else {
                // End track
                isPlaying = false;
                break;
            }
        }
        
        audioBufferPos.store(currentPos);
    }
    
    // Apply EQ
    processEqualizerBuffer(out, framesPerBuffer, TARGET_CHANNELS);
    
    return paContinue;
}

void initAudioPlayer() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        std::cerr << "No default output device found" << std::endl;
        return;
    }
    
    outputParameters.channelCount = TARGET_CHANNELS;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    
    err = Pa_OpenStream(&audioStream,
                       nullptr, // no input
                       &outputParameters,
                       static_cast<double>(TARGET_SAMPLE_RATE),
                       FRAMES_PER_BUFFER,
                       paClipOff,
                       audioCallback,
                       nullptr);
    
    if (err != paNoError) {
        std::cerr << "PortAudio stream open failed: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    
    err = Pa_StartStream(audioStream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream start failed: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    
    // Init Eq
    initEqualizer(TARGET_SAMPLE_RATE);
    
    playlist.clear();
    currentTrackIndex = -1;
    loadVolumeFromFile();
    
    std::cout << "Audio player initialized with PortAudio (SR: " << TARGET_SAMPLE_RATE 
              << " Hz, Channels: " << TARGET_CHANNELS << ")" << std::endl;
}

void shutdownAudio() {
    if (audioStream) {
        Pa_CloseStream(audioStream);
        audioStream = nullptr;
    }
    Pa_Terminate();
    shutdownEqualizer();
    std::cout << "Audio player shutdown" << std::endl;
}

// Playlist
void addTrack(const std::string& filepath) {
    Track t;
    t.filepath = filepath;
    t.durationSeconds = getTrackDuration(filepath);
    playlist.push_back(t);

    if (selectedPlaylistIndex >= 0 && selectedPlaylistIndex < (int)playlists.size()) {
        playlists[selectedPlaylistIndex].tracks.push_back(t);
        savePlaylistsToFile();
    }
}

const std::vector<Track>& getPlaylist() { 
    return playlist; 
}

void clearPlaylist() {
    playlist.clear();
    currentTrackIndex = -1;
    stop();
    shuffleHistory.clear();
}

void pause() {
    if (!isPlaying || isPaused) return;
    isPaused = true;
    std::cout << "Playback paused" << std::endl;
}

void resume() {
    if (!isPlaying || !isPaused) return;
    isPaused = false;
    std::cout << "Playback resumed" << std::endl;
}

void stop() {
    std::lock_guard<std::mutex> lock(audioMutex);
    isPlaying = false;
    isPaused = false;
    audioBufferPos = 0;
    audioBuffer.clear();
    currentTrackPosition = 0.0f;
    std::cout << "Playback stopped" << std::endl;
}

bool isPlaying_f() {
    return isPlaying && !isPaused;
}

bool isPaused_f() {
    return isPlaying && isPaused;
}

void playTrack(int index) {
    if (index < 0 || index >= (int)playlist.size()) return;
    
    stop();
    
    const auto& track = playlist[index];
    
    std::cout << "Loading track: " << track.filepath << std::endl;
    
    AudioInfo audioInfo = getAudioInfo(track.filepath);
    if (!audioInfo.valid) {
        std::cerr << "Failed to get audio info for: " << track.filepath << std::endl;
        return;
    }
    
    std::cout << "  Audio info - SR: " << audioInfo.sampleRate 
              << " Hz, Channels: " << audioInfo.channels 
              << ", Duration: " << audioInfo.duration << "s" << std::endl;
    
    // Loading audio data via ffmpeg
    std::vector<float> rawAudioData = loadAudioWithFFmpeg(track.filepath);
    if (rawAudioData.empty()) {
        std::cerr << "Failed to load audio data for: " << track.filepath << std::endl;
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        
        audioBuffer = std::move(rawAudioData);
        audioBufferPos = 0;
        
        float calculatedDuration = static_cast<float>(audioBuffer.size() / TARGET_CHANNELS) / TARGET_SAMPLE_RATE;
        
        std::cout << "  Loaded buffer: " << audioBuffer.size() / TARGET_CHANNELS 
                  << " frames (" << calculatedDuration << "s)" << std::endl;
        
        // Checking the duration compliance
        if (std::abs(calculatedDuration - audioInfo.duration) > 1.0f) {
            std::cout << "  WARNING: Duration mismatch - calculated: " << calculatedDuration 
                      << "s, expected: " << audioInfo.duration << "s" << std::endl;
        }
    }
    
    // Updating the state
    currentTrackIndex = index;
    selectedTrack = index;
    currentTrackDuration = audioInfo.duration;
    currentTrackPosition = 0.0f;
    isPlaying = true;
    isPaused = false;
    
    // Load cover
    std::string coverPath;
    if (extractCoverFromMP3(track.filepath, coverPath))
        loadTrackCover(coverPath);
    else
        loadTrackCover((resourcePath / "unknown.png").string().c_str());

    // Load lyrics
    if (!loadingLyrics)
        loadLyricsAsync(track.filepath);
    
    // Update shuffle
    if (shuffleEnabled) {
        if (std::find(shuffleHistory.begin(), shuffleHistory.end(), index) == shuffleHistory.end())
            shuffleHistory.push_back(index);
    }
    
    std::cout << "Now playing: " << track.filepath << std::endl;
}

void playPrevious() {
    if (playlist.empty()) return;
    
    if (shuffleEnabled) {
        int newIndex = currentTrackIndex;
        int attempts = 0;
        while (newIndex == currentTrackIndex && playlist.size() > 1 && attempts++ < 100) {
            newIndex = rand() % playlist.size();
        }
        playTrack(newIndex);
    } else {
        int prevIndex = currentTrackIndex - 1;
        if (prevIndex < 0) prevIndex = playlist.size() - 1;
        playTrack(prevIndex);
    }
}

void playNext() {
    if (playlist.empty()) return;

    if (shuffleEnabled) {
        if ((int)shuffleHistory.size() >= (int)playlist.size()) shuffleHistory.clear();
        
        int newIndex = currentTrackIndex;
        int attempts = 0;
        while ((std::find(shuffleHistory.begin(), shuffleHistory.end(), newIndex) != shuffleHistory.end()
                || newIndex == currentTrackIndex) && attempts++ < 100) {
            newIndex = rand() % playlist.size();
        }
        playTrack(newIndex);
    } else {
        int nextIndex = currentTrackIndex + 1;
        if (nextIndex >= (int)playlist.size()) {
            if (repeatEnabled) {
                nextIndex = 0;
            } else {
                stop();
                return;
            }
        }
        playTrack(nextIndex);
    }
}

void toggleShuffle() {
    shuffleEnabled = !shuffleEnabled;
    if (!shuffleEnabled) shuffleHistory.clear();
    std::cout << "Shuffle " << (shuffleEnabled ? "enabled" : "disabled") << std::endl;
}

bool isShuffleEnabled() { 
    return shuffleEnabled; 
}

void seekTo(float seconds) {
    if (!audioBuffer.empty() && seconds >= 0.0f && seconds <= currentTrackDuration) {
        std::lock_guard<std::mutex> lock(audioMutex);
        
        size_t newPos = static_cast<size_t>(seconds * TARGET_SAMPLE_RATE * TARGET_CHANNELS);
        
        newPos = (newPos / TARGET_CHANNELS) * TARGET_CHANNELS;
        
        if (newPos < audioBuffer.size()) {
            audioBufferPos = newPos;
            currentTrackPosition = seconds;
            
            std::cout << "Seeked to: " << seconds << "s (buffer pos: " << newPos << ")" << std::endl;
        }
    }
}

void updateTrackPosition() {
    if (isPlaying && !isPaused && !isSeeking && !audioBuffer.empty()) {
        size_t currentPos = audioBufferPos.load();
        float bufferPosition = static_cast<float>(currentPos / TARGET_CHANNELS) / TARGET_SAMPLE_RATE;
        
        currentTrackPosition = bufferPosition;
        
        if (currentTrackPosition > currentTrackDuration) {
            currentTrackPosition = currentTrackDuration;
        }
    }
}

void updatePlayback() {
    if (isSeeking) return;

    // Check if the track is over
    if (isPlaying && !isPaused && !audioBuffer.empty() && 
        audioBufferPos >= audioBuffer.size() - TARGET_CHANNELS) {
        
        std::cout << "Track finished" << std::endl;
        
        if (currentTrackIndex >= 0) {
            if (repeatEnabled) {
                playTrack(currentTrackIndex);
            } else {
                playNext();
            }
        } else {
            stop();
        }
    }
}

void verifyPlayback() {
    if (!audioBuffer.empty() && isPlaying) {
        size_t currentPos = audioBufferPos.load();
        float expectedDuration = static_cast<float>(audioBuffer.size() / TARGET_CHANNELS) / TARGET_SAMPLE_RATE;
        float currentTime = static_cast<float>(currentPos / TARGET_CHANNELS) / TARGET_SAMPLE_RATE;
        
        std::cout << "Playback verification:" << std::endl;
        std::cout << "  Buffer size: " << audioBuffer.size() << " samples" << std::endl;
        std::cout << "  Expected duration: " << expectedDuration << "s" << std::endl;
        std::cout << "  Current time: " << currentTime << "s" << std::endl;
        std::cout << "  Track duration: " << currentTrackDuration << "s" << std::endl;
    }
}

// Functions for working with settings
void loadVolumeFromFile() {
    fs::path configDir = fs::path(VOLUME_CONFIG_PATH).parent_path();
    if (!fs::exists(configDir)) fs::create_directories(configDir);

    std::ifstream inFile(VOLUME_CONFIG_PATH);
    if (!inFile.good()) {
        float defaultVolume = 0.5f;
        std::ofstream outFile(VOLUME_CONFIG_PATH);
        outFile << defaultVolume;
        current_volume = static_cast<int>(defaultVolume * 128);
        return;
    }

    float volume = 0.5f;
    inFile >> volume;
    current_volume = static_cast<int>(volume * 128);
    std::cout << "Loaded volume: " << volume << std::endl;
}

void saveVolumeToFile() {
    std::ofstream out(VOLUME_CONFIG_PATH);
    float vol = current_volume / 128.0f;
    out << vol;
    std::cout << "Saved volume: " << vol << std::endl;
}

float getTrackDuration(const std::string& filepath) {
    AudioInfo info = getAudioInfo(filepath);
    return info.duration;
}

// JSON
void to_json(json& j, const Track& t) {
    j = json{{"filepath", t.filepath}, {"durationSeconds", t.durationSeconds}};
}

void from_json(const json& j, Track& t) {
    j.at("filepath").get_to(t.filepath);
    j.at("durationSeconds").get_to(t.durationSeconds);
}

void to_json(json& j, const Playlist& p) {
    j = json{{"name", p.name}, {"tracks", p.tracks}};
}

void from_json(const json& j, Playlist& p) {
    j.at("name").get_to(p.name);
    j.at("tracks").get_to(p.tracks);
}

void savePlaylistsToFile() {
    fs::path configDir = fs::path(K_PLAYLIST_FILENAME).parent_path();
    if (!fs::exists(configDir)) fs::create_directories(configDir);
    
    json j = playlists;
    std::ofstream file(K_PLAYLIST_FILENAME);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
        std::cout << "Playlists saved to " << K_PLAYLIST_FILENAME << std::endl;
    }
}

void loadPlaylistsFromFile() {
    std::ifstream file(K_PLAYLIST_FILENAME);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            playlists = j.get<std::vector<Playlist>>();
            file.close();
            std::cout << "Loaded " << playlists.size() << " playlists" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading playlists: " << e.what() << std::endl;
            playlists.clear();
        }
    }
}

const std::vector<Playlist>& getPlaylists() {
    return playlists;
}

std::vector<Playlist>& getPlaylistsMutable() {
    return playlists;
}

void addPlaylist(const std::string& name) {
    for (const auto& p : playlists) {
        if (p.name == name) return;
    }
    Playlist p;
    p.name = name;
    playlists.push_back(p);
    savePlaylistsToFile();
    std::cout << "Added playlist: " << name << std::endl;
}

void addTrackToPlaylist(const std::string& playlistName, const Track& track) {
    for (auto& p : playlists) {
        if (p.name == playlistName) {
            p.tracks.push_back(track);
            savePlaylistsToFile();
            if (selectedPlaylistIndex >= 0 && p.name == playlists[selectedPlaylistIndex].name) {
                playlist.push_back(track);
            }
            std::cout << "Added track to playlist " << playlistName << ": " << track.filepath << std::endl;
            return;
        }
    }
}

void selectPlaylist(int index) {
    if (index >= 0 && index < (int)playlists.size()) {
        selectedPlaylistIndex = index;
        playlist = playlists[index].tracks;
        playlistChanged = true;
        std::cout << "Selected playlist: " << playlists[index].name 
                  << " (" << playlist.size() << " tracks)" << std::endl;
    }
}

void addTrackToSelectedPlaylist(const Track& track) {
    if (selectedPlaylistIndex < 0 || selectedPlaylistIndex >= (int)playlists.size()) return;
    
    playlists[selectedPlaylistIndex].tracks.push_back(track);
    playlist.push_back(track);
    savePlaylistsToFile();
    std::cout << "Added track to selected playlist: " << track.filepath << std::endl;
}

