#include "equalizer_ui.h"
#include "eq.h"
#include "imgui.h"
#include "player.h"

#include <vector>
#include <string>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <filesystem>                                                                                                                                                                                                                                    

namespace fs = std::filesystem;

static bool eqLoaded = false;  // Flag to load config once
static bool enabled = true; // Global flag to save state between launches
static std::string eqConfigPath = (configPath / "eq.cfg").string();
static std::vector<float> eqBands = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

const char* freqLabels[] = { "30Hz", "150Hz", "350Hz", "600Hz", "1KHz", "3.5KHz", "7KHz", "11KHz", "16KHz" };
const int bandCount = (int)eqBands.size();

void loadEQConfig() {
    std::ifstream file(eqConfigPath);
    if (!file.is_open()) {
        std::cerr << "Could not open EQ config file for reading: " << eqConfigPath << std::endl;
        return;
    }

    if (!(file >> enabled)) {
        std::cerr << "Error reading EQ enabled state\n";
        enabled = true;
    }
    setEqualizerEnabled(enabled);

    // Get eq bands
    for (size_t i = 0; i < eqBands.size(); ++i) {
        float val;
        if (!(file >> val)) {
            std::cerr << "Error reading EQ value at band " << i << std::endl;
            break;
        }
        eqBands[i] = val;
        setEqualizerBand(i, val); // Apply
    }
    file.close();
}

void saveEQConfig() {
    std::ofstream file(eqConfigPath);
    if (!file.is_open()) {
        std::cerr << "Could not open EQ config file for writing: " << eqConfigPath << std::endl;
        return;
    }

    // Save enabled
    file << enabled << "\n";

    // Sabe bands
    for (float val : eqBands) {
        file << val << " ";
    }
    file << std::endl;

    file.close();
}

void drawEqualizerUI() {
    if (!eqLoaded) {
        loadEQConfig();
        eqLoaded = true;
    }

    if (ImGui::Checkbox("Enable Equalizer", &enabled)) {
        setEqualizerEnabled(enabled);
        saveEQConfig();
    }

    ImGui::Separator();

    if (!enabled) {
        ImVec2 windowSize = ImGui::GetWindowSize();

        ImGui::SetWindowFontScale(1.3f);

        const char* disabledText = "Equalizer is disabled";
        ImVec2 textSize = ImGui::CalcTextSize(disabledText);

        float textX = (windowSize.x - textSize.x) * 0.5f;
        float textY = (windowSize.y - textSize.y) * 0.5f;

        ImGui::SetCursorPos(ImVec2(textX, textY));
        ImGui::TextDisabled("%s", disabledText);

        ImGui::SetWindowFontScale(1.0f);
        return;
    }

    const float sliderWidth = 20.0f;
    const float sliderHeight = 150.0f;
    const float spacing = 50.0f;

    ImVec2 windowSize = ImGui::GetWindowSize();
    float totalWidth = bandCount * sliderWidth + (bandCount - 1) * spacing;
    float startX = (windowSize.x - totalWidth) * 0.5f;
    float startY = (windowSize.y - sliderHeight - 120.0f) * 0.5f;

    ImGuiStyle& style = ImGui::GetStyle();
    float oldGrabMinSize = style.GrabMinSize;
    ImVec2 oldFramePadding = style.FramePadding;
    ImVec4 oldButtonBg = style.Colors[ImGuiCol_Button];
    ImVec4 oldButtonBgHovered = style.Colors[ImGuiCol_ButtonHovered];
    ImVec4 oldButtonBgActive = style.Colors[ImGuiCol_ButtonActive];
    ImVec4 oldButtonBorder = style.Colors[ImGuiCol_Border];

    style.GrabMinSize = 16.0f;
    style.FramePadding = ImVec2(0, 0);

    ImGui::BeginGroup();

    for (int i = 0; i < bandCount; ++i) {
        float x = startX + i * (sliderWidth + spacing);
        ImGui::SetCursorPos(ImVec2(x, startY));

        ImGui::BeginGroup();

        char valueBuf[16];
        snprintf(valueBuf, sizeof(valueBuf), "%.1f", eqBands[i]);
        float valueTextWidth = ImGui::CalcTextSize(valueBuf).x;
        ImGui::SetCursorPosX(x + (sliderWidth - valueTextWidth) * 0.5f);
        ImGui::Text("%s", valueBuf);

        ImGui::SetCursorPosX(x);
        ImGui::PushID(i);

        float oldValue = eqBands[i];
        if (ImGui::VSliderFloat("##eq", ImVec2(sliderWidth, sliderHeight), &eqBands[i], -30.0f, 30.0f, "")) {
            if (eqBands[i] < -30.0f) eqBands[i] = -30.0f;
            if (eqBands[i] > 30.0f) eqBands[i] = 30.0f;
            setEqualizerBand(i, eqBands[i]);
            saveEQConfig();
        }

        ImGui::PopID();

        float labelWidth = ImGui::CalcTextSize(freqLabels[i]).x;
        ImGui::SetCursorPosX(x + (sliderWidth - labelWidth) * 0.5f);
        ImGui::Text("%s", freqLabels[i]);

        ImGui::EndGroup();
    }

    ImGui::EndGroup();

    ImGui::SetCursorPosY(startY + sliderHeight + 100.0f);

    float buttonsWidth = 3 * 120.0f + 2 * 20.0f;
    ImGui::SetCursorPosX((windowSize.x - buttonsWidth) * 0.5f);

    style.Colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.4f, 0.4f, 0.3f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.5f, 0.5f, 0.5f, 0.3f);
    style.FramePadding = ImVec2(10, 10);

    ImVec2 oldPadding = style.FramePadding;
    style.FramePadding = ImVec2(oldPadding.x, 2);

    ImGui::SetWindowFontScale(1.2f);

    // Boost Bass
    ImGui::PushID("BoostBass");
    if (ImGui::Button("Boost Bass", ImVec2(120, 40))) {
        float rockPreset[] = { 5.0f, 3.0f, 2.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        for (int i = 0; i < bandCount; ++i) {
            eqBands[i] = rockPreset[i];
            setEqualizerBand(i, eqBands[i]);
        }
        saveEQConfig();
    }
    ImGui::PopID();

    ImGui::SameLine(0, 20);

    // Reset
    ImGui::PushID("Reset");
    if (ImGui::Button("Reset", ImVec2(120, 40))) {
        for (int i = 0; i < bandCount; ++i) {
            eqBands[i] = 0.0f;
            setEqualizerBand(i, 0.0f);
        }
        saveEQConfig();
    }
    ImGui::PopID();

    ImGui::SameLine(0, 20);

    // Boost Hight
    ImGui::PushID("BoostHight");
    if (ImGui::Button("Boost Hight", ImVec2(120, 40))) {
        float classicalPreset[] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 5.0f, 6.0f };
        for (int i = 0; i < bandCount; ++i) {
            eqBands[i] = classicalPreset[i];
            setEqualizerBand(i, eqBands[i]);
        }
        saveEQConfig();
    }
    ImGui::PopID();

    ImGui::SetWindowFontScale(1.0f);

    style.GrabMinSize = oldGrabMinSize;
    style.FramePadding = oldFramePadding;
    style.Colors[ImGuiCol_Button] = oldButtonBg;
    style.Colors[ImGuiCol_ButtonHovered] = oldButtonBgHovered;
    style.Colors[ImGuiCol_ButtonActive] = oldButtonBgActive;
    style.Colors[ImGuiCol_Border] = oldButtonBorder;
}

