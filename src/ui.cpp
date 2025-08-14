#include "equalizer_ui.h"
#include "get_artist_info.h"
#include "ui.h"
#include "lyrics.h"
#include "player.h"
#include "tinyfiledialogs.h"
#include "imgui.h"
#include "texture_loader.h"
#include "stb_image.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

static int selectedPlaylistUIIndex = -1;
static const char* glsl_version = "#version 130";
static std::string coverPath = (fs::path(PROJECT_ROOT_DIR) / "resources" / "unknown.png").string();

ImFont* handwrittenFont = nullptr;
ImFont* handwrittenFontYaboku = nullptr; 

int coverWidth = 0;
int coverHeight = 0;

GLFWwindow* window = nullptr;

GLuint iconPlay = 0;
GLuint iconPause = 0;
GLuint iconNext = 0;
GLuint iconPrev = 0;
GLuint iconShuffle = 0;
GLuint iconRepeat = 0;
GLuint iconRepeatOn = 0;
GLuint iconShuffleOn = 0;
GLuint iconShuffleHover = 0;
GLuint iconShuffleOnHover = 0;
GLuint iconPlayHover = 0;
GLuint iconPauseHover = 0;
GLuint iconNextHover = 0;
GLuint iconPrevHover = 0;
GLuint iconRepeatHover = 0;
GLuint iconRepeatOnHover = 0;
GLuint currentTrackCoverTexture = 0;

void loadTrackCover(const std::string& path) {
    // Clearing the previous cover
    if (currentTrackCoverTexture != 0) {
        glDeleteTextures(1, &currentTrackCoverTexture);
        currentTrackCoverTexture = 0;
    }

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        std::cerr << "Failed to load cover image: " << path << std::endl;
        return;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    currentTrackCoverTexture = textureID;
    coverWidth = width;
    coverHeight = height;
}

void initUITextures() {
    fs::path resourcePath = fs::path(PROJECT_ROOT_DIR) / "resources" / "icons";
    iconPlay = loadTexture((resourcePath / "play.png").string().c_str());
    iconPause = loadTexture((resourcePath / "pause.png").string().c_str());
    iconNext = loadTexture((resourcePath / "next.png").string().c_str());
    iconPrev = loadTexture((resourcePath / "prev.png").string().c_str());
    iconShuffle = loadTexture((resourcePath / "shuffle.png").string().c_str());
    iconRepeat = loadTexture((resourcePath / "repeat.png").string().c_str());
    iconRepeatOn = loadTexture((resourcePath / "repeat_on.png").string().c_str());
    iconShuffleOn = loadTexture((resourcePath / "shuffle_on.png").string().c_str());
    iconPlayHover = loadTexture((resourcePath / "play_hover.png").string().c_str());
    iconPauseHover = loadTexture((resourcePath / "pause_hover.png").string().c_str());
    iconNextHover = loadTexture((resourcePath / "next_hover.png").string().c_str());
    iconPrevHover = loadTexture((resourcePath / "prev_hover.png").string().c_str());
    iconRepeatHover = loadTexture((resourcePath / "repeat_on.png").string().c_str());
    iconShuffleHover = loadTexture((resourcePath / "shuffle_on.png").string().c_str());
    iconRepeatOnHover = iconRepeatOn;
    iconShuffleOnHover = iconShuffleOn;

}

bool DrawIconButton(const char* id, ImTextureID iconDefault, ImTextureID iconHovered, ImVec2 size, std::function<void()> onClick) {
    ImGui::InvisibleButton(id, size);

    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 end = ImVec2(pos.x + size.x, pos.y + size.y);

    draw_list->AddImage(hovered ? iconHovered : iconDefault, pos, end);

    if (clicked) {
        onClick();
        return true;
    }

    return false;
}

std::string getFileNameWithoutExtension(const std::string& filepath) {
    size_t slash_pos = filepath.find_last_of("/\\");
    std::string filename = (slash_pos == std::string::npos) ? filepath : filepath.substr(slash_pos + 1);

    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        filename = filename.substr(0, dot_pos);
    }
    return filename;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void OnDrop(GLFWwindow* window, int count, const char** paths) {
        std::cout << "OnDrop called, files count: " << count << std::endl;

        for (int i = 0; i < count; ++i) {
        std::cout << "Dropped file: " << paths[i] << std::endl;
        addTrack(paths[i]);
    }
}

void initWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Настройки для разных ОС
#ifdef __linux__
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "YabokuPlayer");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "yaboku_player");
#elif defined(__APPLE__)
    // macOS специфичные настройки
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#elif defined(_WIN32)
    // Windows специфичные настройки (если нужны)
    // glfwWindowHint(GLFW_WIN32_KEYBOARD_MENU, GLFW_FALSE);
#endif

    window = glfwCreateWindow(900, 600, "Yaboku Player", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        exit(1);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetDropCallback(window, OnDrop);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    loadWindowIcon();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::vector<fs::path> fontBasePaths;
    
#ifdef _WIN32
    fontBasePaths = {
        fs::path(PROJECT_ROOT_DIR) / "resources" / "fonts",
        fs::path("resources") / "fonts",
        fs::path(".") / "resources" / "fonts"
    };
#elif defined(__APPLE__)
    fontBasePaths = {
        fs::path(PROJECT_ROOT_DIR) / "resources" / "fonts",
        fs::path("../Resources/fonts"),
        fs::path("./resources/fonts")
    };
#else
    fontBasePaths = {
        fs::path(PROJECT_ROOT_DIR) / "resources" / "fonts",
        fs::path("./resources/fonts"),
        fs::path("/usr/share/yaboku_player/fonts"),
        fs::path("/usr/local/share/yaboku_player/fonts")
    };
#endif

    fs::path fontPath;
    bool fontPathFound = false;
    
    for (const auto& path : fontBasePaths) {
        if (fs::exists(path)) {
            fontPath = path;
            fontPathFound = true;
            std::cout << "Found fonts directory: " << fontPath << std::endl;
            break;
        }
    }
    
    if (!fontPathFound) {
        std::cerr << "Fonts directory not found in any of the expected locations" << std::endl;
        fontPath = fontBasePaths[0];
    }

    // Load main font
    fs::path mainFontPath = fontPath / "Oswald-VariableFont_wght.ttf";
    ImFont* font = nullptr;
    if (fs::exists(mainFontPath)) {
        font = io.Fonts->AddFontFromFileTTF(
            mainFontPath.string().c_str(),
            24.0f, nullptr,
            io.Fonts->GetGlyphRangesCyrillic()
        );
        std::cout << "Main font loaded successfully" << std::endl;
    } else {
        std::cerr << "Font file not found: " << mainFontPath << std::endl;
    }

    // Load handwritten font
    fs::path handwrittenFontPath = fontPath / "PlaywriteAUQLD-VariableFont_wght.ttf";
    if (fs::exists(handwrittenFontPath)) {
        handwrittenFont = io.Fonts->AddFontFromFileTTF(
            handwrittenFontPath.string().c_str(),
            36.0f, nullptr,
            io.Fonts->GetGlyphRangesCyrillic()
        );
        std::cout << "Handwritten font loaded successfully" << std::endl;
    } else {
        std::cerr << "Font file not found: " << handwrittenFontPath << std::endl;
    }

    // Load Yaboku font
    fs::path yabokuFontPath = fontPath / "FleurDeLeah-Regular.ttf";
    if (fs::exists(yabokuFontPath)) {
        handwrittenFontYaboku = io.Fonts->AddFontFromFileTTF(
            yabokuFontPath.string().c_str(),
            40.0f, nullptr,
            io.Fonts->GetGlyphRangesCyrillic()
        );
        std::cout << "Yaboku font loaded successfully" << std::endl;
    } else {
        std::cerr << "Font file not found: " << yabokuFontPath << std::endl;
    }

    // Check if any font failed to load
    if (!font || !handwrittenFont || !handwrittenFontYaboku) {
        std::cerr << "Failed to load one or more fonts" << std::endl;
        std::cerr << "Main font: " << (font ? "OK" : "FAILED") << std::endl;
        std::cerr << "Handwritten font: " << (handwrittenFont ? "OK" : "FAILED") << std::endl;
        std::cerr << "Yaboku font: " << (handwrittenFontYaboku ? "OK" : "FAILED") << std::endl;
    }
}

void loadWindowIcon() {
    std::vector<fs::path> iconPaths;
    
#ifdef _WIN32
    iconPaths = {
        fs::path(PROJECT_ROOT_DIR) / "resources" / "icons" / "icon.png",
        fs::path("resources") / "icons" / "icon.png",
        fs::path(".") / "resources" / "icons" / "icon.png",
        fs::path("./icon.png"),
        fs::path("../resources/icons/icon.png")
    };
#elif defined(__APPLE__)
    iconPaths = {
        fs::path(PROJECT_ROOT_DIR) / "resources" / "icons" / "icon.png",
        fs::path("../Resources/icon.png"),
        fs::path("./resources/icons/icon.png"),
        fs::path("../../../resources/icons/icon.png"),
        fs::path("./icon.png")
    };
#else
    iconPaths = {
        fs::path(PROJECT_ROOT_DIR) / "resources" / "icons" / "icon.png",
        fs::path("./resources/icons/icon.png"),
        fs::path("/usr/share/pixmaps/yaboku_player.png"),
        fs::path("/usr/share/icons/hicolor/128x128/apps/yaboku_player.png"),
        fs::path("/usr/local/share/pixmaps/yaboku_player.png"),
        fs::path("./icon.png")
    };
#endif

    bool iconLoaded = false;
    for (const auto& iconPath : iconPaths) {
        std::cout << "Trying to load icon from: " << iconPath << std::endl;
        
        if (fs::exists(iconPath)) {
            std::cout << "Icon file found, loading..." << std::endl;
            int width, height, channels;
            unsigned char* iconData = stbi_load(iconPath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (iconData) {
                std::cout << "Icon loaded: " << width << "x" << height << " pixels" << std::endl;
                GLFWimage images[1];
                images[0].width = width;
                images[0].height = height;
                images[0].pixels = iconData;
                glfwSetWindowIcon(window, 1, images);
                stbi_image_free(iconData);
                std::cout << "Window icon set successfully from: " << iconPath << std::endl;
                iconLoaded = true;
                break;
            } else {
                std::cerr << "Failed to load icon data from: " << iconPath << std::endl;
                std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
            }
        }
    }
    
    if (!iconLoaded) {
        std::cerr << "Could not load icon from any path" << std::endl;
        std::cerr << "Tried paths:" << std::endl;
        for (const auto& path : iconPaths) {
            std::cerr << "  - " << path << " (exists: " << fs::exists(path) << ")" << std::endl;
        }
    }
}

bool shouldClose() {
    return glfwWindowShouldClose(window);
}

void startFrame() {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void drawControlPanel(float windowWidth, float windowHeight, float playlistWidth, float coverHeight) {
    ImGui::SetNextWindowPos(ImVec2(playlistWidth, coverHeight));
    ImGui::SetNextWindowSize(ImVec2(windowWidth - playlistWidth, windowHeight - coverHeight));
    ImGui::Begin("Control Panel", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse
    );

    float panelWidth = ImGui::GetWindowSize().x;
    float panelHeight = ImGui::GetWindowSize().y;
    
    // ======== SEEK BAR =========
    {
    static float seekPos = 0.0f;
    static bool wasActive = false;

    float barWidth = panelWidth * 0.63f;
    float barX = (panelWidth - barWidth) * 0.5f;
    float barY = 14.0f;

    int currentMin = static_cast<int>(currentTrackPosition) / 60;
    int currentSec = static_cast<int>(currentTrackPosition) % 60;
    char currentTime[16];
    snprintf(currentTime, sizeof(currentTime), "%02d:%02d", currentMin, currentSec);

    int totalMin = static_cast<int>(currentTrackDuration) / 60;
    int totalSec = static_cast<int>(currentTrackDuration) % 60;
    char totalTime[16];
    snprintf(totalTime, sizeof(totalTime), "%02d:%02d", totalMin, totalSec);

    float textOffset = 50.0f;
    ImGui::SetCursorPos(ImVec2(barX - textOffset + 8, barY - 4.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f,0.6f,0.6f,1.0f));
    ImGui::SetWindowFontScale(0.85f);
    ImGui::Text("%s", currentTime);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGuiStyle& style = ImGui::GetStyle();
    float oldGrabMinSize = style.GrabMinSize;
    ImVec2 oldFramePadding = style.FramePadding;

    style.GrabMinSize = 0.0f;
    style.FramePadding = ImVec2(0, -5);

    ImGui::SetCursorPos(ImVec2(barX, barY));
    ImGui::PushItemWidth(barWidth);

    // Update seekPos from the current track position,
    // only if the slider is NOT active (so as not to erase the position when moving)
    if (!wasActive)
        seekPos = currentTrackPosition;

    if (ImGui::SliderFloat("##SeekBar", &seekPos, 0.0f, std::max(currentTrackDuration, 1.0f), "")) {
        // Wait for the mouse to be released
    }

    // ======== SEEK BAR FILLING =========
    float progress = (currentTrackDuration > 0.0f) ? (seekPos / currentTrackDuration) : 0.0f;
    progress = std::clamp(progress, 0.0f, 1.0f);

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImVec2 filledMax = ImVec2(p0.x + (p1.x - p0.x) * progress, p1.y);

    ImU32 fillColor = IM_COL32(255, 255, 255, 255);
    ImGui::GetWindowDrawList()->AddRectFilled(p0, filledMax, fillColor, style.FrameRounding);

    bool isActive = ImGui::IsItemActive();
    if (wasActive && !isActive && currentTrackDuration > 0.0f) {
        // Check that seekPos is within acceptable limits
        if (seekPos >= 0.0f && seekPos <= currentTrackDuration) {
            isSeeking = true;
            seekTo(seekPos);
            isSeeking = false;
        }
    }
    wasActive = isActive;

    // Checking end of the track
        if (!isSeeking && currentTrackDuration > 0.0f && currentTrackPosition >= currentTrackDuration - 1.0f && isPlaying_f()) {
            std::cout << "SeekBar: Track ended, currentTrackPosition: " << currentTrackPosition 
                      << ", currentTrackDuration: " << currentTrackDuration << std::endl;
            if (!playlist.empty()) {
                if (repeatEnabled) {
                    std::cout << "SeekBar: Repeating track " << currentTrackIndex << std::endl;
                    playTrack(currentTrackIndex);
                } else {
                    std::cout << "SeekBar: Playing next track" << std::endl;
                    playNext();
                }
            } else {
                std::cout << "SeekBar: Stopping due to empty playlist" << std::endl;
                stop();
            }
        }

    ImGui::PopItemWidth();

    style.GrabMinSize = oldGrabMinSize;
    style.FramePadding = oldFramePadding;

    ImVec2 sliderMin = ImGui::GetItemRectMin();
    ImVec2 sliderSize = ImGui::GetItemRectSize();
    ImVec2 timePos = ImVec2(sliderMin.x + sliderSize.x + 12.0f, sliderMin.y - 4.0f);
    ImGui::SetCursorScreenPos(timePos);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f,0.6f,0.6f,1.0f));
    ImGui::SetWindowFontScale(0.85f);
    ImGui::Text("%s", totalTime);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
}

   // ======== BUTTONS =========
    float padding = 33.0f;
    float btnSize = 64.0f;
    float iconSmall = 32.0f;
    float iconPlayBtnSize = 48.0f;

    float totalButtonWidth =
        iconSmall + padding +       // shuffle
        iconSmall + padding +       // prev
        iconPlayBtnSize + padding + // play/pause
        iconSmall + padding +       // next
        iconSmall;                  // repeat

    float startX = (panelWidth - totalButtonWidth) * 0.5f;
    float startY = 45.0f;

    ImGui::SetCursorPos(ImVec2(startX, startY));
    ImGui::BeginGroup();

        DrawIconButton("shuffle_btn",
        isShuffleEnabled() ? iconShuffleOn : iconShuffle,
        isShuffleEnabled() ? iconShuffleOnHover : iconShuffleHover,
        ImVec2(30, 30), []() {
            toggleShuffle();
        });

    ImGui::SameLine(0.0f, padding);

    DrawIconButton("prev_btn", iconPrev, iconPrevHover, ImVec2(30, 30), []() {
        playPrevious();
    });

    ImGui::SameLine(0.0f, padding);

    float currentY = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(currentY - (iconPlayBtnSize - iconSmall) * 0.5f);

    DrawIconButton("play_btn",
    isPlaying_f() ? iconPause : iconPlay,
    isPlaying_f() ? iconPauseHover : iconPlayHover, 
    ImVec2(48, 48), []() {
        if (isPlaying_f()) {
            pause();
        } else if (isPaused_f()) {
            resume();
        } else {
            if (selectedTrack >= 0 && selectedTrack < (int)playlist.size()) {
                playTrack(selectedTrack);
                
                std::string trackPath = playlist[selectedTrack].filepath;
                std::string coverPath;

                if (extractCoverFromMP3(trackPath, coverPath)) {
                    loadTrackCover(coverPath); // load cover from audiofile
                } else {
                    loadTrackCover((resourcePath / "unknown.png").string().c_str());
                }
            } else if (!playlist.empty()) {
                selectedTrack = 0;
                playTrack(0);
                
                std::string trackPath = playlist[selectedTrack].filepath;
                std::string coverPath;

                if (extractCoverFromMP3(trackPath, coverPath)) {
                    loadTrackCover(coverPath);
                } else {
                    loadTrackCover((resourcePath / "unknown.png").string().c_str());
                }
            }
        }
    });

    ImGui::SameLine(0.0f, padding);

    DrawIconButton("next_btn", iconNext, iconNextHover, ImVec2(30, 30), []() {
        playNext();
    });

    ImGui::SameLine(0.0f, padding);

    DrawIconButton("repeat_btn",
        repeatEnabled ? iconRepeatOn : iconRepeat,
        repeatEnabled ? iconRepeatOnHover : iconRepeatHover,
        ImVec2(30, 30), []() {
            repeatEnabled = !repeatEnabled;
        });


    ImGui::EndGroup();

    // === Volume slider ===
    float normalized = getNormalizedVolume();
    float sliderHeight = panelHeight * 0.7f;
    float sliderX = panelWidth - 30.0f;
    float sliderY = (panelHeight - sliderHeight) * 0.5f;

    ImVec2 sliderPos = ImVec2(sliderX, sliderY);
    ImVec2 sliderSize = ImVec2(8.0f, sliderHeight);

    ImGui::SetCursorPos(sliderPos);

    ImGui::InvisibleButton("##volume", sliderSize);

    // Checking for mouse click on slider
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
        float mouseY = ImGui::GetMousePos().y;
        float localTop = ImGui::GetItemRectMin().y;
        float relative = 1.0f - (mouseY - localTop) / sliderSize.y;
        normalized = std::clamp(relative, 0.0f, 0.6f);
        setVolume(normalized);
    }

    // Get the position for drawing
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();

    // Draw the slider frame transparent
    drawList->AddRect(p0, p1, IM_COL32(180, 180, 180, 0), 2.0f);

    // Get the fill height depending on the volume
    float fillHeight = sliderSize.y * (normalized / 0.6f);
    ImVec2 fillP0 = ImVec2(p0.x + 1, p1.y - fillHeight);
    ImVec2 fillP1 = ImVec2(p1.x - 1, p1.y); 

    // Draw the volume fill
    drawList->AddRectFilled(fillP0, fillP1, IM_COL32(225, 225, 225, 225), 2.0f);

    ImGui::End();
}

void drawUI() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize = io.DisplaySize;

    static float playlistWidth = 240.0f;
    static float splitterWidth = 4.0f;
    static int openedPlaylistIndex = -1;  // -1 - list with playlists(not with audio)
    static int selectedTrackInPlaylist = -1;

    float controlPanelHeight = 100.0f;
    float coverHeight = windowSize.y - controlPanelHeight;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(150, windowSize.y), ImVec2(600, windowSize.y));
    ImGui::SetNextWindowSize(ImVec2(playlistWidth, windowSize.y));

    if (ImGui::Begin("Playlist", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove))
    {
        const char* title = "Playlists";
        ImVec2 textSize = ImGui::CalcTextSize(title);
        float windowWidth = ImGui::GetWindowWidth();
        float textPosX = (windowWidth - textSize.x) * 0.5f;

        ImGui::SetCursorPosX(textPosX);

        ImVec2 textStart = ImGui::GetCursorScreenPos();
        ImVec2 textEnd = ImVec2(textStart.x + textSize.x, textStart.y + textSize.y);

        ImVec4 textColor = ImVec4(1.f, 1.f, 1.f, 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::Separator();

        if (openedPlaylistIndex == -1) {
            // List of playlists 
            auto& playlists = getPlaylistsMutable();
            for (int i = 0; i < (int)playlists.size(); ++i) {
                bool isSelected = (i == selectedPlaylistUIIndex);
                if (ImGui::Selectable(playlists[i].name.c_str(), isSelected)) {
                    selectedPlaylistUIIndex = i;
                    selectedTrackInPlaylist = -1;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    openedPlaylistIndex = i; // Открываем плейлист
                    selectedTrackInPlaylist = -1;
                    playlist = playlists[i].tracks;
                }
            // Context menu for deleting a playlist
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete playlist")) {
                playlists.erase(playlists.begin() + i);
                savePlaylistsToFile();

                // Reser choise
                if (selectedPlaylistUIIndex == i) selectedPlaylistUIIndex = -1;
                if (openedPlaylistIndex == i) openedPlaylistIndex = -1;

                ImGui::EndPopup();
                break;
            }
            ImGui::EndPopup();
        }
            }
            ImGui::Separator();
            
            float buttonWidth = 120.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // transparent background
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0)); 
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));  

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 0.5f));
            bool clicked = ImGui::Button("Add Playlist", ImVec2(buttonWidth, 0));
            ImGui::PopStyleColor(); // for text

            ImGui::PopStyleColor(3); // for buttons

            // Draw text on top with a bright color on hover
            if (ImGui::IsItemHovered()) {
                ImVec2 pos = ImGui::GetItemRectMin();
                ImVec2 size = ImGui::GetItemRectSize();
                ImVec2 textSize = ImGui::CalcTextSize("Add Playlist");
                ImVec2 textPos = ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(255,255,255,255), "Add Playlist");
            }

            if (clicked) {
                static char newPlaylistName[64] = "";
                ImGui::OpenPopup("AddPlaylistPopup");
            }

            if (ImGui::BeginPopup("AddPlaylistPopup")) {
                static char newPlaylistName[64] = "";
                ImGui::InputText("Name", newPlaylistName, sizeof(newPlaylistName));
                if (ImGui::Button("Add")) {
                    if (strlen(newPlaylistName) > 0) {
                        addPlaylist(newPlaylistName);
                        ImGui::CloseCurrentPopup();
                        newPlaylistName[0] = 0;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                    newPlaylistName[0] = 0;
                }
                ImGui::EndPopup();
            }
        } else {
            auto& playlists = getPlaylistsMutable();
            if (openedPlaylistIndex >= (int)playlists.size()) {
                openedPlaylistIndex = -1;
                ImGui::End();
                return;
            }

            if (ImGui::Button("< Back")) {
                openedPlaylistIndex = -1;
                selectedTrackInPlaylist = -1;
            }
            ImGui::Separator();
            
            float windowWidth = ImGui::GetWindowWidth();
            float buttonWidth = 120.0f;
            ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

            // "Add Audio"
            ImVec4 normalText = ImVec4(1.f, 1.f, 1.f, 0.5f);
            ImVec4 hoverText  = ImVec4(1.f, 1.f, 1.f, 1.f);

            // Transparent Button Styles
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0, 0, 0, 0));

            // Select text color depending on hover
            ImVec4 textColor = ImGui::IsMouseHoveringRect(
                ImGui::GetCursorScreenPos(),
                ImVec2(ImGui::GetCursorScreenPos().x + buttonWidth,
                       ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight())
            ) ? hoverText : normalText;

            ImGui::PushStyleColor(ImGuiCol_Text, textColor);

            // Draw buttin
            bool clicked = ImGui::Button("Add audio", ImVec2(buttonWidth, 0));

            ImGui::PopStyleColor(4); // Text + 3 buttons


            // Button click handle
            if (clicked) {
                if (openedPlaylistIndex == -1) {
                    std::cout << "No playlist opened, cannot add track\n";
                } else {
                    const char* filters[] = { "*.mp3", "*.wav", "*.ogg" };
                    const char* file = tinyfd_openFileDialog("Выберите аудиофайл", "", 3, filters, NULL, 0);
                    if (file) {
                        if (openedPlaylistIndex >= 0 && openedPlaylistIndex < (int)playlists.size()) {
                            std::string playlistName = playlists[openedPlaylistIndex].name;
                            std::cout << "Adding track to playlist: " << playlistName << "\n";
                            addTrackToPlaylist(playlistName, {file, getTrackDuration(file)});
                            playlist = playlists[openedPlaylistIndex].tracks;
                            selectedTrackInPlaylist = -1;
                        } else {
                            std::cerr << "Invalid playlist index\n";
                        }
                    } else {
                        std::cout << "Диалог отменён\n";
                    }
                }
            }

            ImGui::Separator();

            // Synchronize the selection with the current track
            if (currentTrackIndex >= 0 && currentTrackIndex < (int)playlist.size()) {
                selectedTrackInPlaylist = currentTrackIndex;
            } else {
                selectedTrackInPlaylist = -1;
            }

            for (int i = 0; i < (int)playlist.size(); ++i) {
                const auto& track = playlist[i];
                bool isSelected = (i == selectedTrackInPlaylist);

                // Unique ID for each Selectable
                ImGui::PushID(i);

                ImGui::Selectable(getFileNameWithoutExtension(track.filepath).c_str(), isSelected);

                // Play track by double click
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    selectedTrackInPlaylist = i;
                    selectedTrack = i;
                    playTrack(i);
                    
                    std::string trackPath = playlist[selectedTrack].filepath;
                    std::string coverPath;
                    
                    if (extractCoverFromMP3(trackPath, coverPath)) {
                        loadTrackCover(coverPath);
                    } else {
                        loadTrackCover((resourcePath / "unknown.png").string().c_str());
                    }
                }

                // Right-click context menu for delete
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete from playlist")) {
                        auto& playlists = getPlaylistsMutable();
                        playlists[openedPlaylistIndex].tracks.erase(playlists[openedPlaylistIndex].tracks.begin() + i);

                        // Update playlist
                        playlist = playlists[openedPlaylistIndex].tracks;

                        // save
                        savePlaylistsToFile();

                        // drop the selection if deleted the current one
                        if (selectedTrackInPlaylist == i) {
                            selectedTrackInPlaylist = -1;
                            selectedTrack = -1;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::End();

    // === Track Cover ===
    ImGui::SetNextWindowPos(ImVec2(playlistWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(windowSize.x - playlistWidth, coverHeight));

    ImGui::Begin("Track Cover", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoScrollbar);

    ImVec2 winSize = ImGui::GetWindowSize();

    // center TabBar
    float tabBarWidth = 300.0f;
    ImGui::SetCursorPosX((winSize.x - tabBarWidth) * 0.5f);

    if (ImGui::BeginTabBar("TrackTabs", 
        ImGuiTabBarFlags_NoCloseWithMiddleMouseButton |
        ImGuiTabBarFlags_FittingPolicyResizeDown |
        ImGuiTabBarFlags_NoTabListScrollingButtons))
        {
            if (ImGui::BeginTabItem("Play")) {
                // Track title
                std::string trackTitle = (selectedTrack >= 0 && selectedTrack < (int)playlist.size())
                    ? getFileNameWithoutExtension(playlist[selectedTrack].filepath)
                    : " ";

                ImVec2 textSize = ImGui::CalcTextSize(trackTitle.c_str());
                ImGui::SetCursorPosX((winSize.x - textSize.x) * 0.5f);

                ImGui::PushFont(io.Fonts->Fonts[0]); // main font
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
                ImGui::SetWindowFontScale(1.2f);
                ImGui::TextUnformatted(trackTitle.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleVar();
                ImGui::PopFont();

                ImGui::Dummy(ImVec2(0, 0));

                float squareSize = 0.7f * (winSize.x < winSize.y ? winSize.x : winSize.y);
                ImGui::SetCursorPosX((winSize.x - squareSize) * 0.5f);

                if (currentTrackCoverTexture != 0) {
                    ImGui::Image((void*)(intptr_t)currentTrackCoverTexture, ImVec2(squareSize, squareSize));
                } else {
                    ImGui::Text(" ");
                }

                if (selectedTrack >= 0 && selectedTrack < (int)playlist.size()) {
                    std::string currentTrackPath = playlist[selectedTrack].filepath;

                    if (currentTrackPath != lastTrackPath && !loadingLyrics) {
                        lastTrackPath = currentTrackPath;
                        loadLyricsAsync(currentTrackPath);
                    }

                    ImGui::Spacing();

                    if (loadingLyrics) {
                        const char* text = "Loading lyrics...";
                        ImVec2 loadingSize = ImGui::CalcTextSize(text);
                        ImGui::SetCursorPosX((winSize.x - loadingSize.x) * 0.5f);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                        ImGui::TextUnformatted(text);
                        ImGui::PopStyleColor();
                    } else if (!lyricsLines.empty() && currentLineIndex < (int)lyricsLines.size()) {
                        std::string currentLine = lyricsLines[currentLineIndex];

                        ImVec2 lineSize = ImGui::CalcTextSize(currentLine.c_str());
                        float nextButtonWidth = 50.0f;
                        float totalWidth = lineSize.x + nextButtonWidth;
                        float startX = (winSize.x - totalWidth) * 0.5f;
                        ImGui::SetCursorPosX(startX);

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                        ImGui::TextUnformatted(currentLine.c_str());
                        ImGui::PopStyleColor();

                        ImGui::SameLine();
                        
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.4f);

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.3f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.3f));

                        if (ImGui::Button("Next")) {
                            if (currentLineIndex + 1 < (int)lyricsLines.size()) {
                                currentLineIndex++;
                            }
                        }
                        ImGui::PopStyleColor(4);
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("EQ")) {
                drawEqualizerUI();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("About Artist")) {
                static std::string lastQueriedArtist;
                ImVec2 aboutSize = ImGui::GetWindowSize();

                if (selectedTrack >= 0 && selectedTrack < (int)playlist.size()) {
                    std::string artist = getArtistFromFile(playlist[selectedTrack].filepath);

                    if (artist != lastQueriedArtist && !loadingArtistInfo) {
                        lastQueriedArtist = artist;
                        loadArtistInfoAsync(artist);
                    }

                    if (loadingArtistInfo) {
                        const char* msg = "Loading artist information...";
                        ImVec2 baseSize = ImGui::CalcTextSize(msg);
                        ImVec2 textSize(baseSize.x * 1.3f, baseSize.y * 1.3f);
                        ImVec2 pos((aboutSize.x - textSize.x) * 0.5f, (aboutSize.y - textSize.y) * 0.5f);
                        ImGui::SetCursorPos(pos);
                        ImGui::SetWindowFontScale(1.3f);
                        ImGui::TextDisabled("%s", msg);
                        ImGui::SetWindowFontScale(1.0f);  // Reset the scale back
                    } else {
                        std::lock_guard<std::mutex> lock(artistInfoMutex);
                        if (!artistBio.empty()) {
                            ImGui::PushFont(handwrittenFont);
                            ImGui::TextWrapped("%s", artistBio.c_str());
                            ImGui::PopFont();
                        } else {
                            const char* msg = "No artist info.";
                            ImVec2 baseSize = ImGui::CalcTextSize(msg);
                            ImVec2 textSize(baseSize.x * 1.3f, baseSize.y * 1.3f);
                            ImVec2 pos((aboutSize.x - textSize.x) * 0.5f, (aboutSize.y - textSize.y) * 0.5f);
                            ImGui::SetCursorPos(pos);
                            ImGui::SetWindowFontScale(1.3f);
                            ImGui::TextDisabled("%s", msg);
                            ImGui::SetWindowFontScale(1.0f);  // Reset the scale back
                        }
                    }
                } else {
                    const char* msg = "Please, select track.";
                    ImVec2 baseSize = ImGui::CalcTextSize(msg);
                    ImVec2 textSize(baseSize.x * 1.3f, baseSize.y * 1.3f);
                    ImVec2 pos((aboutSize.x - textSize.x) * 0.5f, (aboutSize.y - textSize.y) * 0.5f);
                    ImGui::SetCursorPos(pos);
                    ImGui::SetWindowFontScale(1.3f);
                    ImGui::TextDisabled("%s", msg);
                    ImGui::SetWindowFontScale(1.0f);  // Reset the scale back
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        
            {
    const char* linkText = "Yaboku";
    
    // Save the current font for calculation
    ImFont* currentFont = ImGui::GetFont();
    
    // Temporarily install the Yaboku font to calculate the correct size
    ImGui::PushFont(handwrittenFontYaboku);
    float textWidth = ImGui::CalcTextSize(linkText).x;
    float textHeight = ImGui::CalcTextSize(linkText).y;
    ImGui::PopFont();
    
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    
    ImVec2 linkPos = ImVec2(windowSize.x - textWidth - 7, 8);
    
    ImGui::SetCursorPos(linkPos);

    ImGui::PushFont(handwrittenFontYaboku);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));

    // Hover and Click Test
    ImVec2 screenPos = ImGui::GetCursorScreenPos();
    if (ImGui::IsMouseHoveringRect(screenPos, ImVec2(screenPos.x + textWidth, screenPos.y + textHeight)))
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            #if defined(_WIN32)
                system("start https://github.com/Yabokua?tab=repositories");
            #elif defined(__APPLE__)
                system("open https://github.com/Yabokua?tab=repositories");
            #else
                system("xdg-open https://github.com/Yabokua?tab=repositories &");
            #endif
        }
    }

    ImGui::TextUnformatted(linkText);

    ImGui::PopStyleColor();
    ImGui::PopFont();
}

    ImGui::End();
    }    
    drawControlPanel(io.DisplaySize.x, io.DisplaySize.y, playlistWidth, coverHeight);
}

std::string getArtistFromFile(const std::string& path) {
    auto name = getFileNameWithoutExtension(path);
    auto sep = name.find(" - ");
    return (sep != std::string::npos) ? name.substr(0, sep) : "";
}

std::string getTitleFromFile(const std::string& path) {
    auto name = getFileNameWithoutExtension(path);
    auto sep = name.find(" - ");
    return (sep != std::string::npos) ? name.substr(sep + 3) : name;
}

void renderFrame() {
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

void shutdownUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

