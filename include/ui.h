#pragma once
#include <string>

struct GLFWwindow;

using GLuint = unsigned int;

extern GLFWwindow* window;
extern GLuint iconPlay;
extern GLuint iconPause;
extern GLuint iconNext;
extern GLuint iconPrev;
extern GLuint iconShuffle;
extern GLuint iconRepeat;
extern GLuint iconRepeatOn;
extern GLuint iconShuffleOn;
extern GLuint iconPlayHover;
extern GLuint iconPauseHover;
extern GLuint iconPrevHover;
extern GLuint iconNextHover;
extern GLuint iconRepeatOnHover;
extern GLuint iconRepeatHover;
extern GLuint iconShuffleOnHover;
extern GLuint iconShuffleHover;

std::string getArtistFromFile(const std::string& path);
std::string getTitleFromFile(const std::string& path);
std::string getFileNameWithoutExtension(const std::string& filepath);
void initWindow();
bool shouldClose();
void startFrame();
void drawUI();
void renderFrame();
void shutdownUI();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void drawControlPanel(float windowWidth, float windowHeight, float playlistWidth, float coverHeight);
void initUITextures();
void loadTrackCover(const std::string& trackFilePath);
void drawEqualizerUI();
void loadWindowIcon();

