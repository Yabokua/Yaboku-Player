#include "eq.h"
#include <cmath>
#include <algorithm>
#include <iostream>

std::unique_ptr<Equalizer> g_equalizer = nullptr;

BiquadFilter::BiquadFilter() : b0(1.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f),
                               x1(0.0f), x2(0.0f), y1(0.0f), y2(0.0f) {}

void BiquadFilter::setPeakingEQ(float frequency, float sampleRate, float gainDB, float Q) {
    float A = std::pow(10.0f, gainDB / 40.0f);
    float omega = 2.0f * M_PI * frequency / sampleRate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float alpha = sin_omega / (2.0f * Q);

    float a0 = 1.0f + alpha / A;

    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cos_omega) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cos_omega) / a0;
    a2 = (1.0f - alpha / A) / a0;
}

void BiquadFilter::setLowShelf(float frequency, float sampleRate, float gainDB, float Q) {
    float A = std::pow(10.0f, gainDB / 40.0f);
    float omega = 2.0f * M_PI * frequency / sampleRate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float S = 1.0f; // shelf slope parameter (1 is max.)
    float beta = std::sqrt(A) / Q;

    float a0 = (A + 1.0f) + (A - 1.0f) * cos_omega + beta * sin_omega;

    b0 = (A * ((A + 1.0f) - (A - 1.0f) * cos_omega + beta * sin_omega)) / a0;
    b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega)) / a0;
    b2 = (A * ((A + 1.0f) - (A - 1.0f) * cos_omega - beta * sin_omega)) / a0;
    a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega)) / a0;
    a2 = ((A + 1.0f) + (A - 1.0f) * cos_omega - beta * sin_omega) / a0;
}

void BiquadFilter::setHighShelf(float frequency, float sampleRate, float gainDB, float Q) {
    float A = std::pow(10.0f, gainDB / 40.0f);
    float omega = 2.0f * M_PI * frequency / sampleRate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float S = 1.0f; // shelf slope parameter (1 is max.)
    float beta = std::sqrt(A) / Q;

    float a0 = (A + 1.0f) - (A - 1.0f) * cos_omega + beta * sin_omega;

    b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_omega + beta * sin_omega)) / a0;
    b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_omega)) / a0;
    b2 = (A * ((A + 1.0f) + (A - 1.0f) * cos_omega - beta * sin_omega)) / a0;
    a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_omega)) / a0;
    a2 = ((A + 1.0f) - (A - 1.0f) * cos_omega - beta * sin_omega) / a0;
}

float BiquadFilter::process(float input) {
    float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

void BiquadFilter::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

Equalizer::Equalizer() : enabled(false), sampleRate(44100.0f), initialized(false) {
    for (int i = 0; i < EQ_BANDS; ++i) {
        bandGains[i] = 0.0f;
    }
}

Equalizer::~Equalizer() {
}

void Equalizer::initialize(float sr) {
    sampleRate = sr;

    // Initialize filters for each band and channel
    for (int band = 0; band < EQ_BANDS; ++band) {
        for (int channel = 0; channel < 2; ++channel) {
            if (band == 0) {
                // First band - Low Shelf
                filters[band][channel].setLowShelf(
                    EQ_FREQUENCIES[band],
                    sampleRate,
                    bandGains[band].load(),
                    0.707f
                );
            } else if (band == EQ_BANDS - 1) {
                // Last band - High Shelf
                filters[band][channel].setHighShelf(
                    EQ_FREQUENCIES[band],
                    sampleRate,
                    bandGains[band].load(),
                    0.707f
                );
            } else {
                // Rest - Peaking EQ
                filters[band][channel].setPeakingEQ(
                    EQ_FREQUENCIES[band],
                    sampleRate,
                    bandGains[band].load(),
                    1.0f
                );
            }
        }
    }

    initialized = true;
    std::cout << "Equalizer initialized with sample rate: " << sampleRate << std::endl;
}

void Equalizer::setBandGain(int band, float gainDB) {
    if (band < 0 || band >= EQ_BANDS) return;

    // Gain range
    gainDB = std::clamp(gainDB, -30.0f, 30.0f);
    bandGains[band] = gainDB;

    if (initialized) {
        // Apply filters
        for (int channel = 0; channel < 2; ++channel) {
            if (band == 0) {
                // First band - Low Shelf
                filters[band][channel].setLowShelf(
                    EQ_FREQUENCIES[band],
                    sampleRate,
                    gainDB,
                    0.707f
                );
            } else if (band == EQ_BANDS - 1) {
                // Last band - High Shelf
                filters[band][channel].setHighShelf(
                    EQ_FREQUENCIES[band],
                    sampleRate,
                    gainDB,
                    0.707f
                );
            } else {
                // Rest - Peaking EQ
                filters[band][channel].setPeakingEQ(
                    EQ_FREQUENCIES[band],
                    sampleRate,
                    gainDB,
                    1.0f
                );
            }
        }
    }

    std::string filterType = (band == 0) ? "Low Shelf" : 
                            (band == EQ_BANDS - 1) ? "High Shelf" : "Peaking";
    std::cout << "EQ Band " << band << " (" << EQ_FREQUENCIES[band] << "Hz, " 
              << filterType << ") set to " << gainDB << "dB" << std::endl;
}

float Equalizer::getBandGain(int band) const {
    if (band < 0 || band >= EQ_BANDS) return 0.0f;
    return bandGains[band].load();
}

void Equalizer::setEnabled(bool en) {
    enabled = en;
    std::cout << "Equalizer " << (en ? "enabled" : "disabled") << std::endl;
}

bool Equalizer::isEnabled() const {
    return enabled.load();
}

void Equalizer::reset() {
    for (int band = 0; band < EQ_BANDS; ++band) {
        setBandGain(band, 0.0f);
        for (int channel = 0; channel < 2; ++channel) {
            filters[band][channel].reset();
        }
    }
    std::cout << "Equalizer reset" << std::endl;
}

void Equalizer::processBuffer(float* buffer, int frames, int channels) {
    if (!enabled.load() || !initialized) return;

    int processChannels = std::min(channels, 2);

    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < processChannels; ++channel) {
            float sample = buffer[frame * channels + channel];

            for (int band = 0; band < EQ_BANDS; ++band) {
                sample = filters[band][channel].process(sample);
            }

            sample = std::clamp(sample, -1.0f, 1.0f);
            buffer[frame * channels + channel] = sample;
        }
    }
}

void initEqualizer(float sampleRate) {
    if (!g_equalizer) {
        g_equalizer = std::make_unique<Equalizer>();
    }
    g_equalizer->initialize(sampleRate);
}

void shutdownEqualizer() {
    g_equalizer.reset();
    std::cout << "Equalizer shutdown" << std::endl;
}

void setEqualizerEnabled(bool enabled) {
    if (g_equalizer) {
        g_equalizer->setEnabled(enabled);
    }
}

bool isEqualizerEnabled() {
    return g_equalizer ? g_equalizer->isEnabled() : false;
}

void setEqualizerBand(int band, float gainDB) {
    if (g_equalizer) {
        g_equalizer->setBandGain(band, gainDB);
    }
}

float getEqualizerBand(int band) {
    return g_equalizer ? g_equalizer->getBandGain(band) : 0.0f;
}

void resetEqualizer() {
    if (g_equalizer) {
        g_equalizer->reset();
    }
}

void processEqualizerBuffer(float* buffer, int frames, int channels) {
    if (g_equalizer) {
        g_equalizer->processBuffer(buffer, frames, channels);
    }
}

