#pragma once

#include <GL/gl.h>
#include <string>

GLuint loadTexture(const char* filename);
bool extractCoverFromMP3(const std::string& mp3Path, std::string& outImagePath);
