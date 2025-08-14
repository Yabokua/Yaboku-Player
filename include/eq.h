#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <memory>

// EQ frequency point count
constexpr int EQ_BANDS = 9;

// Frequency each point
constexpr std::array<float, EQ_BANDS> EQ_FREQUENCIES = {
    30.0f, 150.0f, 350.0f, 600.0f, 1000.0f, 3500.0f, 7000.0f, 11000.0f, 16000.0f
};

class BiquadFilter {
public:
    BiquadFilter();
    void setPeakingEQ(float frequency, float sampleRate, float gainDB, float Q = 1.0f);
    void setLowShelf(float frequency, float sampleRate, float gainDB, float Q = 0.707f);
    void setHighShelf(float frequency, float sampleRate, float gainDB, float Q = 0.707f);
    float process(float input);
    void reset();

private:
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
};

class Equalizer {
public:
    Equalizer();
    ~Equalizer();

    void initialize(float sampleRate);
    void setBandGain(int band, float gainDB);
    float getBandGain(int band) const;
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void reset();

    void processBuffer(float* buffer, int frames, int channels);

private:
    std::array<std::array<BiquadFilter, 2>, EQ_BANDS> filters; // [band][channel]
    std::array<std::atomic<float>, EQ_BANDS> bandGains;
    std::atomic<bool> enabled;
    float sampleRate;
    bool initialized;
};

extern std::unique_ptr<Equalizer> g_equalizer;

// For player 
void initEqualizer(float sampleRate = 44100.0f);
void shutdownEqualizer();
void setEqualizerEnabled(bool enabled);
bool isEqualizerEnabled();
void setEqualizerBand(int band, float gainDB);
float getEqualizerBand(int band);
void resetEqualizer();

void processEqualizerBuffer(float* buffer, int frames, int channels);
