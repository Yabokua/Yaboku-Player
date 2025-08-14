#pragma once

#ifdef _WIN32
#include <GL/gl.h>
#elif __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <string>

GLuint loadTexture(const char* filename);
bool extractCoverFromMP3(const std::string& mp3Path, std::string& outImagePath);

